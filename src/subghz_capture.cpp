#include "subghz_capture.h"
#include "config.h"
#include "pins.h"
#include <Preferences.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>

SubGhzCapture subCapture;

// presets de frequencia pro item "capturar" (a reproducao usa a frequencia
// GRAVADA em cada slot, nao esta lista -- assim nunca troca de banda sozinha)
static const float FREQS[SubGhzCapture::NUM_FREQS] = {433.92f, 315.00f, 390.00f, 868.35f};

// enderecos de registrador do CC1101 (fixos no datasheet; usamos o numero
// direto pra nao depender do nome exato das macros na lib do driver)
static const uint8_t REG_IOCFG0   = 0x02;
static const uint8_t REG_PKTCTRL0 = 0x08;
static const uint8_t REG_MDMCFG2  = 0x12;

static Preferences prefs;

// ============ ISR (grava a duracao de cada pulso em micros) ============
void IRAM_ATTR subcapIsr() {
  uint32_t now = micros();
  uint32_t dur = now - subCapture._capLastEdgeUs;
  subCapture._capLastEdgeUs = now;
  if (!subCapture._capHaveEdge) { subCapture._capHaveEdge = true; return; }  // 1o edge = so referencia
  if (subCapture._capCount < SUBCAP_MAX_PULSES) {
    subCapture._capBuf[subCapture._capCount++] = (dur > 65500) ? 65500 : (uint16_t)dur;
  }
}

// ============ NVS ============
void SubGhzCapture::loadSlot(int i) {
  char key[8]; snprintf(key, sizeof(key), "s%d", i);
  size_t want = sizeof(Slot);
  size_t got = prefs.getBytes(key, &_slots[i], want);
  if (got != want) _slots[i] = Slot();   // nunca gravado / tamanho mudou -> zera
}

void SubGhzCapture::saveSlot(int i) {
  char key[8]; snprintf(key, sizeof(key), "s%d", i);
  prefs.putBytes(key, &_slots[i], sizeof(Slot));
}

void SubGhzCapture::save() {
  for (int i = 0; i < SUBCAP_NUM_SLOTS; i++) saveSlot(i);
}

// ============ CC1101 ============
void SubGhzCapture::armRx() {
  cancelCapture();
  ELECHOUSE_cc1101.Init();
  ELECHOUSE_cc1101.setMHZ(FREQS[_freqIdx]);
  ELECHOUSE_cc1101.SpiWriteReg(REG_MDMCFG2,  0x30);  // OOK/ASK, sem sync word
  ELECHOUSE_cc1101.SpiWriteReg(REG_PKTCTRL0, 0x32);  // formato serial assincrono, comprimento infinito
  ELECHOUSE_cc1101.SpiWriteReg(REG_IOCFG0,   0x0D);  // GDO0 = dado serial assincrono
  ELECHOUSE_cc1101.SetRx();
  pinMode(CC1101_GDO0, INPUT);
}

void SubGhzCapture::begin() {
  static bool opened = false;
  if (!opened) { prefs.begin("subcap", false); opened = true; }
  for (int i = 0; i < SUBCAP_NUM_SLOTS; i++) loadSlot(i);
  armRx();
  _status = "";
}

void SubGhzCapture::nextSlot(int d) {
  _slotIdx = ((_slotIdx + d) % SUBCAP_NUM_SLOTS + SUBCAP_NUM_SLOTS) % SUBCAP_NUM_SLOTS;
}

void SubGhzCapture::nextFreq() {
  _freqIdx = (_freqIdx + 1) % NUM_FREQS;
  armRx();
}

void SubGhzCapture::cancelCapture() {
  if (_capturing) detachInterrupt(digitalPinToInterrupt(CC1101_GDO0));
  _capturing = false;
}

void SubGhzCapture::startCapture() {
  armRx();                    // garante RX + registradores corretos antes de gravar
  _capCount = 0;
  _capHaveEdge = false;
  _captureStartMs = millis();
  _capLastEdgeUs = micros();
  _capturing = true;
  _status = "";
  attachInterrupt(digitalPinToInterrupt(CC1101_GDO0), subcapIsr, CHANGE);
}

void SubGhzCapture::finishCapture() {
  detachInterrupt(digitalPinToInterrupt(CC1101_GDO0));
  _capturing = false;

  uint16_t n = _capCount;
  if (n >= SUBCAP_MIN_PULSES) {
    Slot &s = _slots[_slotIdx];
    s.count = n;
    s.freqMHz = FREQS[_freqIdx];
    for (uint16_t i = 0; i < n; i++) s.pulses[i] = _capBuf[i];
    saveSlot(_slotIdx);
    _status = "Capturado!";
  } else {
    _status = "Nada capturado";
  }
  _lastReact = millis();
  ELECHOUSE_cc1101.SetRx();
}

void SubGhzCapture::tick() {
  if (!_capturing) return;
  uint32_t now = millis();
  bool timedOut = (now - _captureStartMs) >= SUBCAP_TIMEOUT_MS;
  bool full = (_capCount >= SUBCAP_MAX_PULSES);
  bool quietEnough = _capHaveEdge && (_capCount >= SUBCAP_MIN_PULSES) &&
                      ((uint32_t)(micros() - _capLastEdgeUs) > SUBCAP_IDLE_GAP_US);
  if (timedOut || full || quietEnough) finishCapture();
}

void SubGhzCapture::play() {
  Slot &s = _slots[_slotIdx];
  if (s.count < SUBCAP_MIN_PULSES) { _status = "Slot vazio"; _lastReact = millis(); return; }

  cancelCapture();
  ELECHOUSE_cc1101.Init();
  ELECHOUSE_cc1101.setMHZ(s.freqMHz);           // usa a frequencia GRAVADA, nao a selecionada
  ELECHOUSE_cc1101.SpiWriteReg(REG_MDMCFG2,  0x30);
  ELECHOUSE_cc1101.SpiWriteReg(REG_PKTCTRL0, 0x32);
  ELECHOUSE_cc1101.SpiWriteReg(REG_IOCFG0,   0x0D);
  ELECHOUSE_cc1101.SetTx();
  pinMode(CC1101_GDO0, OUTPUT);
  digitalWrite(CC1101_GDO0, LOW);
  delayMicroseconds(200);                        // deixa o PLL/VCO estabilizar antes de bater os pulsos

  bool level = HIGH;   // 1o intervalo gravado = periodo com portadora ligada (GDO0 parte de LOW)
  for (uint16_t i = 0; i < s.count; i++) {
    digitalWrite(CC1101_GDO0, level ? HIGH : LOW);
    delayMicroseconds(s.pulses[i]);
    level = !level;
  }
  digitalWrite(CC1101_GDO0, LOW);

  ELECHOUSE_cc1101.setSidle();
  armRx();                                       // volta a escutar no preset selecionado
  _status = "Reproduzido!";
  _lastReact = millis();
}

// =================================================================== desenho ==
void SubGhzCapture::draw(TFT_eSPI &tft, bool full) {
  if (full) {
    tft.fillScreen(COLOR_BG);
    tft.fillRect(0, 0, tft.width(), 22, COLOR_SEL);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_FG, COLOR_SEL);
    tft.drawString("Sub-GHz Capturar/Repetir", 6, 4, 2);
    tft.setTextColor(COLOR_WARN, COLOR_BG);
    tft.drawString("So SEU equipamento - codigo FIXO (nao rolling code)!", 6, 208, 1);
    tft.setTextColor(COLOR_FG, COLOR_BG);
    tft.drawString("OK=grava  AUX=repete  girar=slot  clique=freq  BACK=sai", 6, 226, 1);
  }

  // sem fillRect geral aqui: cada drawString() ja apaga o texto anterior
  // sozinho (usa a cor de fundo do setTextColor), desde que a string tenha
  // largura fixa -- isso elimina o "flash" que piscava a cada tick (20ms)
  tft.setTextDatum(TL_DATUM);

  char b[48];
  const Slot &s = _slots[_slotIdx];
  bool hasData = s.count >= SUBCAP_MIN_PULSES;

  tft.setTextColor(COLOR_ACCENT, COLOR_BG);
  snprintf(b, sizeof(b), "Slot %d/%d", _slotIdx + 1, SUBCAP_NUM_SLOTS);
  tft.drawString(b, 8, 30, 4);

  tft.setTextColor(hasData ? COLOR_OK : COLOR_FG, COLOR_BG);
  tft.drawString(hasData ? "GRAVADO" : "vazio  ", 170, 34, 2);

  tft.setTextColor(COLOR_FG, COLOR_BG);
  snprintf(b, sizeof(b), "Pulsos gravados: %-4d", s.count);
  tft.drawString(b, 8, 66, 2);

  // largura fixa mesmo vazio (senao, ao esvaziar o slot, sobra o texto antigo
  // dessa linha -- so o fillRect geral escondia isso antes)
  if (hasData) snprintf(b, sizeof(b), "Freq. gravada: %.2f MHz     ", s.freqMHz);
  else         snprintf(b, sizeof(b), "%-28s", "");
  tft.drawString(b, 8, 86, 2);

  tft.setTextColor(COLOR_ACCENT, COLOR_BG);
  snprintf(b, sizeof(b), "Freq. p/ capturar: %.2f MHz", FREQS[_freqIdx]);
  tft.drawString(b, 8, 112, 2);

  if (_capturing) {
    bool blink = (millis() / 250) % 2;
    tft.setTextColor(COLOR_WARN, COLOR_BG);
    tft.drawString(blink ? "GRAVANDO..." : "           ", 8, 150, 4);
  } else {
    bool showStatus = _status[0] && (millis() - _lastReact < 2500);
    tft.setTextColor(COLOR_OK, COLOR_BG);
    snprintf(b, sizeof(b), "%-16s", showStatus ? _status : "");
    tft.drawString(b, 8, 150, 4);
  }
}
