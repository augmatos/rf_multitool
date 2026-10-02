// ============================================================================
//  esphishing - firmware principal (menu + apps de analise de RF)
//  Plataforma: ESP32-S3-DevKitC-1  |  Framework: Arduino  |  Build: PlatformIO
//
//  Navegacao:  gire o encoder = move selecao | clique do encoder ou BTN_OK = entra
//              BTN_BACK = volta/sai do app    | BTN_AUX = acao de contexto
//
//  USO RESPONSAVEL: somente em equipamentos/redes proprios ou autorizados.
// ============================================================================
#include <Arduino.h>
#include <math.h>
#include <SPI.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <ESP32Encoder.h>
#include <RF24.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "pins.h"
#include "config.h"
#include "tamagotchi.h"
#include "beacon_spam.h"
#include "radar.h"
#include "subghz_capture.h"
#include "achievements.h"
#include "rogue_ap.h"
#include "deauth_tool.h"
#include "beacon_spoof.h"

// ---------------------------------------------------------------- objetos ---
TFT_eSPI      tft = TFT_eSPI();
ESP32Encoder  encoder;
RF24          nrf1(NRF1_CE, NRF1_CSN);
RF24          nrf2(NRF2_CE, NRF2_CSN);

// presenca dos modulos (preenchida pelo selfTest)
bool haveCC1101 = false, haveNRF1 = false, haveNRF2 = false;

Tamagotchi pet;   // bichinho de RF (estado persiste na NVS)
BeaconSpam  beacon;
BeaconSpoof spoof;     // pentest: clona SSID+BSSID de AP real
RogueAp     rogueAp;   // SO pentest autorizado -- ver aviso em rogue_ap.h
DeauthTool  deauthTool; // SO pentest autorizado -- ver aviso em deauth_tool.h

// ============================================================ ENTRADA =======
int32_t lastDetent = 0;
int readEncoderDelta() {
  int32_t c = (int32_t)encoder.getCount() / ENC_STEPS_PER_DETENT;
  int d = c - lastDetent;
  lastDetent = c;
  return d;
}
void syncEncoder() { lastDetent = (int32_t)encoder.getCount() / ENC_STEPS_PER_DETENT; }

struct Btn { uint8_t pin; bool last; uint32_t t; };
Btn bOK{BTN_OK, false, 0}, bBack{BTN_BACK, false, 0}, bAux{BTN_AUX, false, 0}, bEnc{ENC_SW, false, 0};

bool pressed(Btn &b) {                       // detector de borda com debounce
  bool now = (digitalRead(b.pin) == LOW);    // ativo em LOW
  bool fired = false;
  if (now && !b.last && (millis() - b.t > 40)) { fired = true; b.t = millis(); }
  b.last = now;
  return fired;
}
bool selectPressed() { return pressed(bOK) | pressed(bEnc); }

// ============================================================ UI ============
void drawHeader(const char *title) {
  tft.fillScreen(COLOR_BG);
  tft.fillRect(0, 0, tft.width(), 22, COLOR_SEL);
  tft.setTextColor(COLOR_FG, COLOR_SEL);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(title, 6, 4, 2);
}

void msg(const char *line, uint16_t color = COLOR_WARN) {
  drawHeader("Aviso");
  tft.setTextColor(color, COLOR_BG);
  tft.drawString(line, 8, 60, 4);
  tft.setTextColor(COLOR_FG, COLOR_BG);
  tft.drawString("[BACK] para voltar", 8, 210, 2);
  while (!pressed(bBack)) delay(20);
}

const char *mainItems[] = {
  "WiFi Scanner",
  "2.4GHz Analyzer (NRF)",
  "Sub-GHz Sweep (CC1101)",
  "Sub-GHz Monitor (CC1101)",
  "Sub-GHz Capturar/Repetir",
  "Waterfall Display",
  "Scanner BLE",
  "Radar de Pessoas",
  "RF Tamagotchi",
  "Beacon Spammer",
  "Beacon Spoofing (PENTEST!)",
  "Rogue AP (PENTEST!)",
  "Deauth (PENTEST!)",
  "Diagnostico / Self-test",
  "Uptime & Sistema",
  "Achievements",
  "Sobre / Uso responsavel"
};
const int N_MAIN = sizeof(mainItems) / sizeof(mainItems[0]);
int sel = 0;

const int MENU_VISIBLE = 9;   // itens que cabem na tela de 240px (o resto rola)
int menuTop = 0;              // indice do primeiro item mostrado (janela de rolagem)

void drawMenu() {
  drawHeader(FW_NAME "  " FW_VERSION);
  tft.setTextDatum(TL_DATUM);

  // mantem o item selecionado sempre dentro da janela visivel
  if (sel < menuTop) menuTop = sel;
  if (sel >= menuTop + MENU_VISIBLE) menuTop = sel - MENU_VISIBLE + 1;

  for (int row = 0; row < MENU_VISIBLE; row++) {
    int i = menuTop + row;
    if (i >= N_MAIN) break;
    int y = 28 + row * 22;
    if (i == sel) {
      tft.fillRect(2, y - 2, tft.width() - 4, 20, COLOR_ACCENT);
      tft.setTextColor(COLOR_BG, COLOR_ACCENT);
    } else {
      tft.setTextColor(COLOR_FG, COLOR_BG);
    }
    tft.drawString(mainItems[i], 10, y, 2);
  }

  // barra de rolagem na borda direita: trilho AZUL + indicador CIANO
  if (N_MAIN > MENU_VISIBLE) {
    const int sbX = tft.width() - 6, sbY = 26, sbW = 4;
    const int sbH = MENU_VISIBLE * 22;                       // altura do trilho
    tft.fillRect(sbX, sbY, sbW, sbH, COLOR_SEL);             // trilho (azul)
    int thumbH = sbH * MENU_VISIBLE / N_MAIN;
    if (thumbH < 10) thumbH = 10;
    int thumbY = sbY + (sbH - thumbH) * menuTop / (N_MAIN - MENU_VISIBLE);
    tft.fillRect(sbX, thumbY, sbW, thumbH, COLOR_ACCENT);    // indicador (ciano)
  }
}

// ============================================================ SELF-TEST =====
void selfTest() {
  // Os 3 radios compartilham o SPI global nos pinos RAD_*
  SPI.begin(RAD_SCK, RAD_MISO, RAD_MOSI);

  // Deseleciona AMBOS os NRF antes de sondar: com o CSN de um solto, ele dirige
  // o MISO e colide com quem estiver sendo lido (deteccao dependia da ordem).
  pinMode(NRF1_CSN, OUTPUT); digitalWrite(NRF1_CSN, HIGH);
  pinMode(NRF2_CSN, OUTPUT); digitalWrite(NRF2_CSN, HIGH);

  ELECHOUSE_cc1101.setSpiPin(RAD_SCK, RAD_MISO, RAD_MOSI, CC1101_CS);
  haveCC1101 = ELECHOUSE_cc1101.getCC1101();   // true se o chip responder
  if (haveCC1101) {
    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.setMHZ(SUBGHZ_DEFAULT_MHZ);
  }

  // O primeiro begin() logo apos energizar pode falhar (rádio ainda nao pronto):
  // tenta algumas vezes antes de dar o modulo como ausente.
  auto detectNRF = [](RF24 &r) -> bool {
    for (int i = 0; i < 5; i++) {
      if (r.begin(&SPI) && r.isChipConnected()) return true;
      delay(100);
    }
    return false;
  };
  haveNRF1 = detectNRF(nrf1);
  haveNRF2 = detectNRF(nrf2);
}

void appDiag() {
  selfTest();
  drawHeader("Diagnostico");
  tft.setTextDatum(TL_DATUM);
  auto line = [&](int y, const char *name, bool ok) {
    tft.setTextColor(COLOR_FG, COLOR_BG);
    tft.drawString(name, 10, y, 4);
    tft.setTextColor(ok ? COLOR_OK : COLOR_WARN, COLOR_BG);
    tft.drawString(ok ? "OK" : "FALHA", 230, y, 4);
  };
  line(34,  "Display ILI9341", true);   // se voce le isto, o display esta OK
  line(64,  "CC1101 sub-GHz", haveCC1101);
  line(94,  "NRF24 #1", haveNRF1);
  line(124, "NRF24 #2", haveNRF2);
  tft.setTextColor(COLOR_FG, COLOR_BG);
  tft.drawString("[BACK] para voltar", 10, 212, 2);
  while (!pressed(bBack)) delay(20);
}

// ============================================================ APP: Uptime ===
void appUptime() {
  static uint32_t minFreeHeap = UINT32_MAX;   // menor heap livre ja visto (detecta vazamento)
  drawHeader("Uptime & Sistema");
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COLOR_FG, COLOR_BG);
  tft.drawString("[BACK] sair", 8, 224, 1);

  while (true) {
    if (pressed(bBack)) return;

    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < minFreeHeap) minFreeHeap = freeHeap;
    uint32_t upS = millis() / 1000;
    uint32_t hh = upS / 3600, mm = (upS % 3600) / 60, ss = upS % 60;
    float tempC = temperatureRead();   // sensor interno do ESP32-S3: nao calibrado, so referencia

    tft.fillRect(0, 24, tft.width(), 196, COLOR_BG);
    char b[48];

    tft.setTextColor(COLOR_ACCENT, COLOR_BG);
    snprintf(b, sizeof(b), "Uptime: %02lu:%02lu:%02lu", (unsigned long)hh, (unsigned long)mm, (unsigned long)ss);
    tft.drawString(b, 8, 32, 4);

    tft.setTextColor(COLOR_FG, COLOR_BG);
    snprintf(b, sizeof(b), "Temp. interna: %.1f C", tempC);
    tft.drawString(b, 8, 68, 2);
    tft.setTextColor(COLOR_WARN, COLOR_BG);
    tft.drawString("(sensor interno, pouco preciso)", 8, 86, 1);

    tft.setTextColor(COLOR_FG, COLOR_BG);
    snprintf(b, sizeof(b), "Heap livre: %lu KB", (unsigned long)(freeHeap / 1024));
    tft.drawString(b, 8, 110, 2);
    snprintf(b, sizeof(b), "Heap livre (minimo): %lu KB", (unsigned long)(minFreeHeap / 1024));
    tft.drawString(b, 8, 130, 2);
    snprintf(b, sizeof(b), "Maior bloco alocavel: %lu KB", (unsigned long)(ESP.getMaxAllocHeap() / 1024));
    tft.drawString(b, 8, 150, 2);

    tft.setTextColor(COLOR_ACCENT, COLOR_BG);
    snprintf(b, sizeof(b), "%s rev%d @ %luMHz", ESP.getChipModel(), ESP.getChipRevision(), (unsigned long)ESP.getCpuFreqMHz());
    tft.drawString(b, 8, 176, 2);
    snprintf(b, sizeof(b), "Flash: %lu MB", (unsigned long)(ESP.getFlashChipSize() / (1024UL * 1024UL)));
    tft.drawString(b, 8, 196, 2);

    delay(500);
  }
}

// ============================================================ APP: Achievements
void appAchievements() {
  ach.draw(tft, true);
  syncEncoder();
  while (true) {
    if (pressed(bBack)) return;
    int d = readEncoderDelta();
    if (d) ach.selMove(d);
    ach.draw(tft, false);
    delay(100);
  }
}

void appAbout() {
  drawHeader("Sobre");
  tft.setTextColor(COLOR_ACCENT, COLOR_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(FW_NAME "  v" FW_VERSION, 10, 34, 4);
  tft.setTextColor(COLOR_FG, COLOR_BG);
  // imprime o aviso de uso responsavel (multilinha)
  int y = 80; String s = RESPONSIBLE_USE; int start = 0;
  while (true) {
    int nl = s.indexOf('\n', start);
    String l = (nl < 0) ? s.substring(start) : s.substring(start, nl);
    tft.drawString(l, 10, y, 2); y += 18;
    if (nl < 0) break; start = nl + 1;
  }
  tft.drawString("[BACK] para voltar", 10, 212, 2);
  while (!pressed(bBack)) delay(20);
}

// ============================================================ APP: WiFi =====
void appWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  while (true) {
    drawHeader("WiFi Scanner");
    tft.setTextColor(COLOR_FG, COLOR_BG);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("Escaneando...", 8, 28, 2);
    int n = WiFi.scanNetworks();
    ach.reportWifiScan(n);
    drawHeader("WiFi Scanner");
    int y = 26;
    for (int i = 0; i < n && i < 11; i++) {
      bool open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
      tft.setTextColor(open ? COLOR_WARN : COLOR_FG, COLOR_BG);
      String l = String(WiFi.RSSI(i)) + " c" + String(WiFi.channel(i)) + " " + WiFi.SSID(i);
      tft.drawString(l.substring(0, 40), 6, y, 2); y += 17;
    }
    tft.setTextColor(COLOR_ACCENT, COLOR_BG);
    tft.drawString(String(n) + " redes  [BACK] sair", 6, 222, 2);
    for (int t = 0; t < WIFI_SCAN_INTERVAL_MS; t += 40) { if (pressed(bBack)) { WiFi.mode(WIFI_OFF); return; } delay(40); }
  }
}

// ============================================================ APP: NRF ======
void appNrf() {
  if (!haveNRF1) { msg("NRF24 #1 ausente"); return; }
  nrf1.setAutoAck(false);
  nrf1.disableCRC();
  nrf1.setDataRate(RF24_1MBPS);
  nrf1.startListening();
  nrf1.stopListening();

  static uint8_t val[NRF_NUM_CHANNELS];
  const int base = 200, reps = 60;
  drawHeader("2.4GHz Analyzer");
  while (true) {
    if (pressed(bBack)) return;
    memset(val, 0, sizeof(val));
    for (int r = 0; r < reps; r++)
      for (int ch = 0; ch < NRF_NUM_CHANNELS; ch++) {
        nrf1.setChannel(ch);
        nrf1.startListening();
        delayMicroseconds(NRF_DWELL_US);
        nrf1.stopListening();
        if (nrf1.testRPD()) val[ch]++;        // RPD: sinal > -64 dBm no canal
      }
    int w = (tft.width() - 8) / NRF_NUM_CHANNELS; if (w < 1) w = 1;
    tft.fillRect(0, 24, tft.width(), base - 24, COLOR_BG);
    for (int ch = 0; ch < NRF_NUM_CHANNELS; ch++) {
      int h = (int)(sqrtf(val[ch] * 12.0f) * 3.5f);  // escala sqrt para amplificar
      if (h > base - 34) h = base - 34;               // clampa no máximo
      uint16_t c = (val[ch] > reps / 2) ? COLOR_WARN : COLOR_ACCENT;
      tft.fillRect(4 + ch * w, base - h, w, h, c);
    }
    tft.setTextColor(COLOR_FG, COLOR_BG);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("2400          canal          2479 MHz   [BACK]", 4, base + 6, 2);

    // DEBUG: mostra o pico de sinal no topo
    uint8_t maxVal = 0;
    int maxCh = -1;
    for (int ch = 0; ch < NRF_NUM_CHANNELS; ch++) {
      if (val[ch] > maxVal) { maxVal = val[ch]; maxCh = ch; }
    }
    tft.setTextColor(COLOR_ACCENT, COLOR_BG);
    char dbg[32];
    snprintf(dbg, sizeof(dbg), "Pico: ch%d=%d/%d", maxCh, maxVal, reps);
    tft.drawString(dbg, 4, 24, 2);
  }
}

// ============================================================ APP: Sweep ====
void appSubSweep() {
  if (!haveCC1101) { msg("CC1101 ausente"); return; }
  ELECHOUSE_cc1101.Init();
  ach.reportSweepUsed();
  const int base = 196;
  static int rssiArr[256];
  drawHeader("Sub-GHz Sweep");
  while (true) {
    if (pressed(bBack)) return;
    int idx = 0, best = -999; float bestF = 0;
    for (float f = SUBGHZ_SWEEP_START; f <= SUBGHZ_SWEEP_END && idx < 256; f += SUBGHZ_SWEEP_STEP) {
      ELECHOUSE_cc1101.setMHZ(f);
      ELECHOUSE_cc1101.SetRx();
      delayMicroseconds(3000);          // espera o PLL/AGC estabilizar (igual antes)
      int r = -127;
      for (int s = 0; s < 5; s++) {     // depois de estabilizado, tenta pegar a rajada
        int v = ELECHOUSE_cc1101.getRssi();
        if (v > r) r = v;
        delayMicroseconds(600);
      }
      rssiArr[idx] = r;
      if (r > best) { best = r; bestF = f; }
      idx++;
    }
    int w = (tft.width() - 8) / (idx > 0 ? idx : 1); if (w < 1) w = 1;
    tft.fillRect(0, 24, tft.width(), base - 24 + 20, COLOR_BG);
    for (int i = 0; i < idx; i++) {
      int h = map(constrain(rssiArr[i], -100, -20), -100, -20, 2, base - 30);
      tft.fillRect(4 + i * w, base - h, w, h, COLOR_OK);
    }
    tft.setTextColor(COLOR_FG, COLOR_BG);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("Pico: " + String(bestF, 2) + " MHz  " + String(best) + " dBm", 4, base + 6, 2);
    tft.drawString(String(SUBGHZ_SWEEP_START, 1) + "                  " + String(SUBGHZ_SWEEP_END, 1) + " MHz  [BACK]", 4, base + 24, 1);
    delay(40);
  }
}

// ============================================================ APP: Monitor ==
void appSubMonitor() {
  if (!haveCC1101) { msg("CC1101 ausente"); return; }
  float f = SUBGHZ_DEFAULT_MHZ;
  ELECHOUSE_cc1101.Init();
  ELECHOUSE_cc1101.setMHZ(f);
  ELECHOUSE_cc1101.SetRx();
  drawHeader("Sub-GHz Monitor");
  syncEncoder();
  tft.setTextColor(COLOR_FG, COLOR_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("gire: +/- 0.01 MHz   [BACK] sair", 8, 220, 2);
  while (true) {
    if (pressed(bBack)) return;
    int d = readEncoderDelta();
    if (d) { f += d * 0.01f; ELECHOUSE_cc1101.setMHZ(f); ELECHOUSE_cc1101.SetRx(); }
    int r = ELECHOUSE_cc1101.getRssi();

    // largura fixa: drawString() ja apaga o texto anterior sozinho (usa a cor
    // de fundo do setTextColor) -- sem isso nao precisamos de fillRect aqui,
    // o que elimina o "flash" preto que piscava a cada atualizacao
    tft.setTextColor(COLOR_ACCENT, COLOR_BG);
    char freqBuf[16];
    snprintf(freqBuf, sizeof(freqBuf), "%7.2f MHz", f);
    tft.drawString(freqBuf, 10, 44, 6);

    // a barra so precisa limpar a propria faixa estreita (nao a tela toda)
    int bar = map(constrain(r, -100, -20), -100, -20, 0, tft.width() - 20);
    tft.fillRect(10, 120, tft.width() - 20, 26, COLOR_BG);
    tft.fillRect(10, 120, bar, 26, (r > -60) ? COLOR_WARN : COLOR_ACCENT);

    tft.setTextColor(COLOR_FG, COLOR_BG);
    char rssiBuf[16];
    snprintf(rssiBuf, sizeof(rssiBuf), "%5d dBm", r);
    tft.drawString(rssiBuf, 10, 156, 4);

    digitalWrite(LED_RF, (r > -65) ? HIGH : LOW);
    delay(60);
  }
}

// ============================================================ APP: Sub-GHz Capture
void appSubGhzCapture() {
  if (!haveCC1101) { msg("CC1101 ausente"); return; }
  WiFi.mode(WIFI_OFF);
  subCapture.begin();
  subCapture.draw(tft, true);
  syncEncoder();
  while (true) {
    if (pressed(bBack)) {
      subCapture.cancelCapture();
      subCapture.save();
      ELECHOUSE_cc1101.setSidle();
      digitalWrite(LED_RF, LOW);
      return;
    }

    if (!subCapture.isCapturing()) {
      int d = readEncoderDelta();
      if (d) subCapture.nextSlot(d);
      if (pressed(bEnc)) subCapture.nextFreq();     // clique do encoder != BTN_OK aqui
      if (pressed(bOK))  { subCapture.startCapture(); ach.reportSubGhzCaptureUsed(); }
      if (pressed(bAux)) subCapture.play();
    }

    subCapture.tick();
    digitalWrite(LED_RF, subCapture.isCapturing() && ((millis() / 150) % 2) ? HIGH : LOW);
    subCapture.draw(tft, false);
    delay(20);
  }
}

// ============================================================ APP: Waterfall
// Mapeia RSSI (-100..-20 dBm) numa cor tipo "espectrograma": azul (fraco) ->
// ciano -> verde -> amarelo -> vermelho (forte).
static uint16_t rssiColor(int rssi) {
  int pct = map(constrain(rssi, -100, -20), -100, -20, 0, 100);
  uint8_t r, g, b;
  if (pct < 25)      { r = 0;   g = map(pct,  0, 25,   0, 255); b = 255; }
  else if (pct < 50) { r = 0;   g = 255;                        b = map(pct, 25, 50, 255, 0); }
  else if (pct < 75) { r = map(pct, 50, 75,   0, 255); g = 255; b = 0; }
  else               { r = 255; g = map(pct, 75, 100, 255,  0); b = 0; }
  return tft.color565(r, g, b);
}

// Fontes suportadas: CC1101 sub-GHz (RSSI real) e NRF24 2.4GHz (RPD virado
// "pseudo-RSSI": conta quantas de N escutas curtas viram portadora no canal).
// Historico fica num buffer em RAM (nao le a tela de volta via SPI, que e
// lento/depende do driver) -- cada tick desloca o buffer e reenvia tudo de
// uma vez com pushImage, dando scroll de verdade em vez de "paginar".
enum WfSource { WF_SUBGHZ, WF_NRF };

void appWaterfall() {
  if (!haveCC1101 && !haveNRF1) { msg("CC1101 e NRF ausentes"); return; }
  WiFi.mode(WIFI_OFF);

  const int PIXW = 260;                  // largura do historico (colunas/pixels)
  const int ROWS = 140;                  // "profundidade" do historico (linhas)
  uint16_t *hist = (uint16_t *)malloc(ROWS * PIXW * sizeof(uint16_t));
  if (!hist) { msg("RAM insuficiente p/ waterfall"); return; }
  memset(hist, 0, ROWS * PIXW * sizeof(uint16_t));   // COLOR_BG == 0x0000

  const int WX = 4, WY = 24;
  WfSource src = haveCC1101 ? WF_SUBGHZ : WF_NRF;
  int bins = 0, colW = 1;

  auto recalcBins = [&]() {
    bins = (src == WF_SUBGHZ)
             ? (int)((SUBGHZ_SWEEP_END - SUBGHZ_SWEEP_START) / WATERFALL_STEP) + 1
             : NRF_NUM_CHANNELS;
    if (bins > PIXW) bins = PIXW;
    colW = PIXW / bins; if (colW < 1) colW = 1;
  };

  auto drawChrome = [&]() {
    drawHeader(src == WF_SUBGHZ ? "Waterfall Sub-GHz (CC1101)" : "Waterfall 2.4GHz (NRF24)");
    tft.fillRect(WX, WY, PIXW, ROWS, COLOR_BG);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_FG, COLOR_BG);
    if (src == WF_SUBGHZ) {
      tft.drawString(String(SUBGHZ_SWEEP_START, 2), WX, WY + ROWS + 4, 1);
      tft.drawString(String(SUBGHZ_SWEEP_END, 2) + " MHz", WX + PIXW - 60, WY + ROWS + 4, 1);
    } else {
      tft.drawString("canal 0", WX, WY + ROWS + 4, 1);
      tft.drawString("canal 79", WX + PIXW - 50, WY + ROWS + 4, 1);
    }
    const int LY = WY + ROWS + 20, LH = 8;
    for (int x = 0; x < PIXW; x++) tft.drawFastVLine(WX + x, LY, LH, rssiColor(map(x, 0, PIXW, -100, -20)));
    tft.drawString("fraco", WX, LY + LH + 2, 1);
    tft.drawString("forte", WX + PIXW - 30, LY + LH + 2, 1);
    tft.drawString(haveCC1101 && haveNRF1 ? "AUX=troca fonte  [BACK] sair" : "[BACK] sair", WX, LY + LH + 16, 1);
  };

  if (haveNRF1) { nrf1.setAutoAck(false); nrf1.disableCRC(); nrf1.setDataRate(RF24_1MBPS); }
  if (haveCC1101) ELECHOUSE_cc1101.Init();
  recalcBins();
  drawChrome();

  uint16_t newRow[PIXW];

  while (true) {
    if (pressed(bBack)) break;

    if (pressed(bAux) && haveCC1101 && haveNRF1) {
      src = (src == WF_SUBGHZ) ? WF_NRF : WF_SUBGHZ;
      recalcBins();
      memset(hist, 0, ROWS * PIXW * sizeof(uint16_t));
      drawChrome();
    }

    for (int x = 0; x < PIXW; x++) newRow[x] = COLOR_BG;
    if (src == WF_SUBGHZ) {
      for (int i = 0; i < bins; i++) {
        float f = SUBGHZ_SWEEP_START + i * WATERFALL_STEP;
        ELECHOUSE_cc1101.setMHZ(f);
        ELECHOUSE_cc1101.SetRx();
        delayMicroseconds(600);
        uint16_t c = rssiColor(ELECHOUSE_cc1101.getRssi());
        for (int p = 0; p < colW; p++) { int x = i * colW + p; if (x < PIXW) newRow[x] = c; }
      }
    } else {
      const int reps = 8;
      for (int ch = 0; ch < bins; ch++) {
        int hits = 0;
        for (int r = 0; r < reps; r++) {
          nrf1.setChannel(ch);
          nrf1.startListening();
          delayMicroseconds(NRF_DWELL_US);
          nrf1.stopListening();
          if (nrf1.testRPD()) hits++;
        }
        uint16_t c = rssiColor(map(hits, 0, reps, -100, -20));
        for (int p = 0; p < colW; p++) { int x = ch * colW + p; if (x < PIXW) newRow[x] = c; }
      }
    }

    memmove(hist + PIXW, hist, (ROWS - 1) * PIXW * sizeof(uint16_t));  // desloca tudo 1 linha
    memcpy(hist, newRow, PIXW * sizeof(uint16_t));                     // nova linha no topo
    tft.pushImage(WX, WY, PIXW, ROWS, hist);

    digitalWrite(LED_RF, (millis() / 200) % 2);
    delay(10);
  }

  free(hist);
}

// ============================================================ APP: BLE Scan =
// So mapeamos o fabricante quando temos certeza do ID (Apple = 0x004C, usado
// em iBeacon/AirTag e por isso bem documentado). Os demais IDs da lista da
// Bluetooth SIG nao foram conferidos aqui de proposito -- melhor mostrar nada
// do que "chutar" um fabricante errado.
static const char *bleCompanyName(uint16_t id) {
  if (id == 0x004C) return "Apple";
  return nullptr;
}

void appBleScan() {
  WiFi.mode(WIFI_OFF);              // libera o radio 2.4GHz pro BLE
  BLEDevice::init("");              // reinit e inofensivo se o Radar ja tiver iniciado o BLE
  BLEScan *scan = BLEDevice::getScan();
  scan->setActiveScan(true);        // pede o nome do dispositivo quando disponivel
  scan->setInterval(100);
  scan->setWindow(99);

  while (true) {
    drawHeader("Scanner BLE");
    tft.setTextColor(COLOR_FG, COLOR_BG);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("Escaneando (" + String(BLE_SCAN_DURATION_S) + "s)...", 8, 28, 2);

    BLEScanResults results = scan->start(BLE_SCAN_DURATION_S, false);
    int n = results.getCount();
    ach.reportBleScanUsed();

    drawHeader("Scanner BLE");
    int y = 26;
    for (int i = 0; i < n && i < 11; i++) {
      BLEAdvertisedDevice d = results.getDevice(i);
      String tag = "";
      if (d.haveManufacturerData()) {
        std::string md = d.getManufacturerData();
        if (md.size() >= 2) {
          uint16_t cid = (uint8_t)md[0] | ((uint16_t)(uint8_t)md[1] << 8);
          const char *nm = bleCompanyName(cid);
          if (nm) tag = String(" [") + nm + "]";
        }
      }
      String name = d.haveName() ? String(d.getName().c_str()) : "(sem nome)";
      String line = String(d.getRSSI()) + "dBm " + name + tag;
      tft.setTextColor(COLOR_FG, COLOR_BG);
      tft.drawString(line.substring(0, 42), 6, y, 2);
      y += 17;
    }
    scan->clearResults();

    tft.setTextColor(COLOR_ACCENT, COLOR_BG);
    tft.drawString(String(n) + " dispositivos  [BACK] sair", 6, 222, 2);

    for (int t = 0; t < BLE_SCAN_PAUSE_MS; t += 40) { if (pressed(bBack)) { scan->stop(); return; } delay(40); }
  }
}

// ============================================================ APP: Tamagotchi
void appTamagotchi() {
  pet.begin();
  pet.addXp(ach.drainXpBonus());   // aplica bonus de conquistas desbloqueadas fora daqui
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.scanNetworks(true);                 // scan assincrono (nao trava a animacao)
  if (haveNRF1) {
    nrf1.setAutoAck(false);
    nrf1.disableCRC();
    nrf1.setDataRate(RF24_1MBPS);
    nrf1.startListening();
    nrf1.stopListening();
  }
  if (haveCC1101) {                        // garante estado limpo (outro app pode ter
    ELECHOUSE_cc1101.Init();               // deixado o CC1101 em modo OOK assincrono, etc)
    ELECHOUSE_cc1101.setMHZ(SUBGHZ_DEFAULT_MHZ);
    ELECHOUSE_cc1101.SetRx();
  }

#if TAMA_BLE_FEED_ENABLED
  BLEDevice::init("");                     // inofensivo se outro app ja tiver iniciado
  BLEScan *bleScan = BLEDevice::getScan();
  bleScan->setActiveScan(false);           // passivo: so contar presenca, mais leve
  bleScan->setInterval(100);
  bleScan->setWindow(99);
  int bleCycle = 0, lastBle = 0;
#endif

  bool showStats = false;
  pet.draw(tft, true, showStats);
  syncEncoder();
  uint32_t lastFeed = 0;
  while (true) {
    if (pressed(bBack)) { pet.save(); WiFi.scanDelete(); WiFi.mode(WIFI_OFF); return; }

    if (pressed(bAux)) { showStats = !showStats; pet.draw(tft, true, showStats); }

    // carinho: clique ou giro do encoder
    if (selectPressed() || readEncoderDelta() != 0) pet.petLove();

    // ciclo de alimentacao a cada 3 s
    if (millis() - lastFeed > 3000) {
      lastFeed = millis();
      int nets = 0;
      int done = WiFi.scanComplete();
      if (done >= 0) { nets = done; WiFi.scanDelete(); WiFi.scanNetworks(true); }
      else if (done == WIFI_SCAN_FAILED) { WiFi.scanNetworks(true); }

      int hits = 0;
      if (haveNRF1) {
        for (int ch = 0; ch < NRF_NUM_CHANNELS; ch++)
          for (int r = 0; r < 6; r++) {
            nrf1.setChannel(ch);
            nrf1.startListening();
            delayMicroseconds(NRF_DWELL_US);
            nrf1.stopListening();
            if (nrf1.testRPD()) { hits++; break; }
          }
      }

      int subHit = 0;
      if (haveCC1101 && ELECHOUSE_cc1101.getRssi() > -60) subHit = 1;  // mesmo limiar do Monitor

      int bleDevices = 0;
#if TAMA_BLE_FEED_ENABLED
      // scan BLE curto (1s, bloqueante) so a cada ~4 ciclos (~12s) -- pausa breve e
      // pouco frequente, pra nao travar a animacao toda hora nem disputar demais
      // o radio 2.4GHz com o WiFi que ja roda nesse app
      if (++bleCycle >= 4) {
        bleCycle = 0;
        BLEScanResults r = bleScan->start(1, false);
        lastBle = r.getCount();
        bleScan->clearResults();
      }
      bleDevices = lastBle;
#endif

      pet.feed(nets, hits, subHit, bleDevices);     // come os sinais do ambiente
    }

    pet.update();
    ach.reportTamagotchiLevel(pet.level());
    pet.draw(tft, false, showStats);
    delay(70);
  }
}

// ============================================================ APP: Beacon ===
// AUX cicla os modos: Engracado -> Historia -> Stress-test. Os dois primeiros
// sao spam continuo de nomes de piada (clique liga/desliga). O Stress-test e
// modo de PENTEST: exige confirmar autorizacao (clique do encoder) e so
// transmite enquanto o botao OK esta SEGURADO (dead-man), com teto de SSIDs e
// cooldown -- nunca um "flood" livre.
void appBeacon() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  beacon.begin();
  beacon.draw(tft, true);
  syncEncoder();
  while (true) {
    if (pressed(bBack)) { beacon.setEnabled(false); WiFi.mode(WIFI_OFF); return; }
    if (pressed(bAux))  beacon.cycleMode();

    if (beacon.isStress()) {
      if (!beacon.stressAuthorized()) {
        if (pressed(bEnc)) beacon.authorizeStress();             // confirma autorizacao
        digitalWrite(LED_RF, LOW);
      } else {
        int d = readEncoderDelta();
        if (d) beacon.adjustStressCount(d);                      // girar = nº de SSIDs
        if (selectPressed()) beacon.setEnabled(!beacon.enabled()); // clique = liga/desliga transmissao
        bool tx = beacon.stressBurst(beacon.enabled());
        digitalWrite(LED_RF, tx ? HIGH : LOW);
      }
    } else {
      if (selectPressed()) beacon.setEnabled(!beacon.enabled()); // clique = liga/desliga
      int d = readEncoderDelta();
      if (d) beacon.scrollPreview(d);                            // girar = rola a previa
      if (beacon.enabled()) { beacon.tick(); digitalWrite(LED_RF, (millis() / 100) % 2); }
      else                    digitalWrite(LED_RF, LOW);
    }

    beacon.draw(tft, false);
    delay(25);
  }
}

// ============================================================ APP: Beacon Spoofing
// Clona SSID + BSSID reais de um AP existente. Pentest: testar se clientes se
// conectam ao primeiro AP com aquele nome (sem validar BSSID). Com limites de
// seguranca: cooldown entre rajadas, confirmacao de autorizacao.
void appBeaconSpoof() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  spoof.begin();
  spoof.draw(tft, true);
  syncEncoder();
  uint32_t lastRescan = 0;
  while (true) {
    if (pressed(bBack)) { WiFi.mode(WIFI_OFF); return; }
    if (pressed(bAux)) {
      // AUX = rescanneia as redes (pode levar uns segundos)
      spoof.scanNets();
      lastRescan = millis();
    }
    if (selectPressed()) spoof.setEnabled(!spoof.enabled());
    int d = readEncoderDelta();
    if (d) spoof.selMove(d);

    if (spoof.enabled()) { spoof.tick(); digitalWrite(LED_RF, (millis() / 100) % 2); }
    else                  digitalWrite(LED_RF, LOW);

    spoof.draw(tft, false);
    delay(50);
  }
}

// ============================================================ APP: Rogue AP =
// SO PENTEST AUTORIZADO POR ESCRITO. Ver aviso completo em rogue_ap.h/.cpp.
// Exige confirmacao explicita TODA VEZ que o app abre; so clona SSID de rede
// ja vista num scan ao vivo (nao aceita nome digitado livremente); nao tem
// portal cativo/captura de credenciais -- so mede associacao automatica.
void appRogueAp() {
  rogueAp.begin();
  rogueAp.draw(tft, true);
  syncEncoder();
  while (true) {
    if (pressed(bBack)) { rogueAp.cancel(); return; }
    if (selectPressed()) rogueAp.confirmStep();
    int d = readEncoderDelta();
    if (d) rogueAp.selMove(d);

    rogueAp.draw(tft, false);   // draw() ja limpa e redesenha o corpo inteiro a cada chamada
    digitalWrite(LED_RF, rogueAp.isRunning() && ((millis() / 200) % 2) ? HIGH : LOW);
    delay(150);
  }
}

// ============================================================ APP: Deauth ===
// SO PENTEST AUTORIZADO POR ESCRITO. Ver aviso completo em deauth_tool.h/.cpp.
// Confirmacao explicita toda vez que o app abre; alvo so de scan ao vivo;
// rajada limitada (DEAUTH_BURST_FRAMES) + cooldown entre disparos -- essas
// duas ultimas sao decisao de engenharia, nao ficaram configuraveis na UI.
void appDeauth() {
  deauthTool.begin();
  deauthTool.draw(tft, true);
  syncEncoder();
  while (true) {
    if (pressed(bBack)) { deauthTool.cancel(); return; }
    if (selectPressed()) deauthTool.confirmStep();
    int d = readEncoderDelta();
    if (d) deauthTool.selMove(d);

    deauthTool.draw(tft, false);
    delay(150);
  }
}

// ============================================================ APP: Radar ===
void appRadar() {
  radar.begin();
  radar.draw(tft, true);
  while (true) {
    if (pressed(bBack)) { radar.end(); WiFi.mode(WIFI_OFF); return; }
    radar.tick();
    radar.draw(tft, false);
    digitalWrite(LED_RF, (millis() / 300) % 2);  // blink suave
    delay(100);
  }
}

// ============================================================ SETUP/LOOP ====
// --- Boot animation: mascote ORIGINAL (sem relacao com nenhum personagem de
//     terceiros) saindo de uma capsula de captura estilizada. ---------------
static void drawOrb(int cx, int cy) {
  const int r = 22;
  tft.fillSmoothCircle(cx, cy, r, 0xFFFF, COLOR_BG);            // base branca
  for (int dy = -r; dy <= 0; dy++) {                            // metade de cima ciano
    int dxx = (int)sqrtf((float)(r * r - dy * dy));
    tft.drawFastHLine(cx - dxx, cy + dy, 2 * dxx + 1, COLOR_ACCENT);
  }
  tft.fillRect(cx - r, cy - 2, 2 * r + 1, 4, COLOR_BG);         // faixa central
  tft.drawSmoothCircle(cx, cy, r, COLOR_FG, COLOR_BG);          // contorno
  tft.fillCircle(cx, cy, 6, COLOR_FG);                          // botao
  tft.drawCircle(cx, cy, 6, COLOR_BG);
  tft.fillCircle(cx, cy, 3, COLOR_BG);
}

static void drawCritter(int cx, int cy, bool blink) {
  const uint16_t body = COLOR_ACCENT;
  tft.drawLine(cx - 14, cy - 26, cx - 24, cy - 48, COLOR_FG);   // antenas-orelha
  tft.drawLine(cx + 14, cy - 26, cx + 24, cy - 48, COLOR_FG);
  tft.fillCircle(cx - 25, cy - 50, 5, COLOR_WARN);
  tft.fillCircle(cx + 25, cy - 50, 5, COLOR_WARN);
  tft.fillSmoothCircle(cx, cy, 32, body, COLOR_BG);             // corpo
  tft.fillSmoothCircle(cx, cy + 12, 15, 0xFFFF, body);          // barriga
  tft.fillSmoothCircle(cx - 15, cy + 28, 7, body, COLOR_BG);    // pezinhos
  tft.fillSmoothCircle(cx + 15, cy + 28, 7, body, COLOR_BG);
  int eyeY = cy - 6, dx = 12;                                   // olhos
  if (blink) {
    tft.drawFastHLine(cx - dx - 5, eyeY, 10, COLOR_FG);
    tft.drawFastHLine(cx + dx - 5, eyeY, 10, COLOR_FG);
  } else {
    tft.fillCircle(cx - dx, eyeY, 5, COLOR_FG);
    tft.fillCircle(cx + dx, eyeY, 5, COLOR_FG);
    tft.fillCircle(cx - dx + 2, eyeY - 2, 2, COLOR_BG);         // brilho
    tft.fillCircle(cx + dx + 2, eyeY - 2, 2, COLOR_BG);
  }
  tft.fillCircle(cx - 20, cy + 2, 3, COLOR_WARN);               // bochechas
  tft.fillCircle(cx + 20, cy + 2, 3, COLOR_WARN);
  tft.drawCircleHelper(cx, cy + 2, 7, 0xC, COLOR_FG);           // sorriso
  tft.drawCircleHelper(cx, cy + 2, 8, 0xC, COLOR_FG);
}

void splash() {
  const int cx = tft.width() / 2;
  const int cy = 108;
  tft.fillScreen(COLOR_BG);

  // FASE 1 -- a capsula chacoalha (o bichinho quer sair)
  uint32_t t0 = millis();
  while (millis() - t0 < 1100) {
    if (selectPressed()) return;                                 // deixa pular a animacao
    float e = (float)(millis() - t0);
    float amp = 5.0f * (0.4f + e / 1100.0f);                     // chacoalho aumentando
    int sx = cx + (int)(amp * sinf(e / 26.0f));
    tft.fillRect(cx - 40, cy - 28, 80, 56, COLOR_BG);
    drawOrb(sx, cy);
    delay(16);
  }

  // FASE 2 -- estouro: aneis de luz expandindo
  tft.fillRect(cx - 40, cy - 40, 80, 80, COLOR_BG);
  for (int r = 4; r <= 78; r += 6) {
    if (selectPressed()) return;
    tft.drawCircle(cx, cy, r, (r / 6) % 2 ? COLOR_ACCENT : COLOR_WARN);
    delay(14);
  }
  tft.fillScreen(COLOR_BG);

  // FASE 3 -- a mascote aparece, balanca, pisca e emite ondas de RF
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COLOR_ACCENT, COLOR_BG);
  tft.drawString(FW_NAME, cx, 186, 4);
  tft.setTextColor(COLOR_WARN, COLOR_BG);
  tft.drawString("Uso responsavel - alvos autorizados", cx, 210, 2);
  tft.setTextColor(COLOR_FG, COLOR_BG);
  tft.drawString("clique para iniciar", cx, 230, 2);
  tft.setTextDatum(TL_DATUM);

  uint32_t t3 = millis();
  while (!selectPressed() && (millis() - t3 < 6000)) {
    uint32_t e = millis() - t3;
    int  bob   = (int)(3 * sinf(e / 260.0f));
    bool blink = (e % 3200) < 150;
    tft.fillRect(cx - 54, cy - 94, 120, 150, COLOR_BG);          // limpa area do bicho + ondas
    int base = (int)((e / 12) % 34);                             // ondas de RF na anteninha direita
    for (int k = 0; k < 3; k++) {
      int wr = base + k * 12;
      if (wr > 4 && wr < 34)
        tft.drawCircleHelper(cx + 25, (cy + bob) - 50, wr, 0x3, COLOR_ACCENT);
    }
    drawCritter(cx, cy + bob, blink);
    delay(16);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  pinMode(BTN_AUX, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  pinMode(LED_STAT, OUTPUT);
  pinMode(LED_RF, OUTPUT);
  pinMode(DISP_BL, OUTPUT);
  digitalWrite(DISP_BL, HIGH);                 // liga backlight

  // KY-040: se NAO compilar, sua versao da lib usa puType::UP (maiusculo)
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoder.attachHalfQuad(ENC_DT, ENC_CLK);
  encoder.clearCount();

  tft.init();
  tft.setRotation(UI_ROTATION);
  tft.fillScreen(COLOR_BG);

  splash();
  selfTest();
  ach.begin();
  syncEncoder();
  drawMenu();
}

void loop() {
  int d = readEncoderDelta();
  if (d) {
    sel = (sel + d) % N_MAIN;
    while (sel < 0) sel += N_MAIN;
    drawMenu();
  }
  if (selectPressed()) {
    switch (sel) {
      case 0: appWifi();            break;
      case 1: appNrf();             break;
      case 2: appSubSweep();        break;
      case 3: appSubMonitor();      break;
      case 4: appSubGhzCapture();   break;
      case 5: appWaterfall();       break;
      case 6: appBleScan();         break;
      case 7: appRadar();           break;
      case 8: appTamagotchi();      break;
      case 9: appBeacon();          break;
      case 10: appBeaconSpoof();    break;
      case 11: appRogueAp();        break;
      case 12: appDeauth();         break;
      case 13: appDiag();           break;
      case 14: appUptime();         break;
      case 15: appAchievements();   break;
      case 16: appAbout();          break;
    }
    syncEncoder();
    drawMenu();
  }
  ach.tick();                                     // acumula uptime total (independente do app aberto)
  digitalWrite(LED_STAT, (millis() / 500) % 2);   // heartbeat
  delay(10);
}
