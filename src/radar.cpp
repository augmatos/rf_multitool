#include "radar.h"
#include "config.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <cstring>

PersonRadar radar;

// ============================================================================
//  Fila de MACs entre as callbacks assíncronas (Wi-Fi promíscuo / BLE) e o
//  loop principal. As callbacks NÃO podem tocar nos std::map (alocam heap e
//  rodam noutra task -> corromperiam o map que o tick() está percorrendo).
//  Aqui elas só copiam escalares para um ring buffer sob spinlock; o tick()
//  drena essa fila para os maps, sempre no contexto do loop principal.
// ============================================================================
struct MacEvent { uint64_t mac; bool ble; };
static const int      Q_SIZE = 128;
static volatile MacEvent _q[Q_SIZE];
static volatile uint16_t _qHead = 0, _qTail = 0;
static portMUX_TYPE   _qMux = portMUX_INITIALIZER_UNLOCKED;

static uint64_t macToUint64(const uint8_t *mac) {
  uint64_t res = 0;
  for (int i = 0; i < 6; i++) res = (res << 8) | mac[i];
  return res;
}

static void pushMac(uint64_t mac, bool ble) {
  portENTER_CRITICAL(&_qMux);
  uint16_t next = (uint16_t)((_qHead + 1) % Q_SIZE);
  if (next != _qTail) {                 // se cheia, descarta (perder 1 MAC não faz mal)
    _q[_qHead].mac = mac;
    _q[_qHead].ble = ble;
    _qHead = next;
  }
  portEXIT_CRITICAL(&_qMux);
}

// ============ callbacks globais (rodam em outras tasks) ============
static void radarWifiCallback(void *buf, wifi_promiscuous_pkt_type_t type) {
  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
  uint8_t *frame = pkt->payload;
  if (!frame || pkt->rx_ctrl.sig_len < 24) return;

  uint16_t frameCtrl = (frame[1] << 8) | frame[0];
  uint8_t subtype = (frameCtrl >> 4) & 0xF;
  if (subtype != 0x4) return;  // subtype 0x4 = probe request

  uint8_t srcMac[6];
  memcpy(srcMac, frame + 10, 6);  // source MAC está no offset 10
  pushMac(macToUint64(srcMac), false);
}

class RadarBleCallback : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice adv) override {
    pushMac(macToUint64(*adv.getAddress().getNative()), true);
  }
};

// scan BLE contínuo (duração 0) nunca "termina", então este callback de
// conclusão nunca é chamado -- existe só para usar a sobrecarga NÃO bloqueante
// de BLEScan::start (a versão (duração, bool) TRAVA para sempre com duração 0).
static void radarScanDone(BLEScanResults) {}

void PersonRadar::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(radarWifiCallback);

  BLEDevice::init("");
  BLEScan *scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new RadarBleCallback(), false);
  scan->setActiveScan(false);
  scan->setInterval(100);
  scan->setWindow(99);
  scan->start(0, radarScanDone, false);  // NÃO bloqueante (0 = contínuo)
}

void PersonRadar::end() {
  esp_wifi_set_promiscuous(false);
  BLEScan *scan = BLEDevice::getScan();
  if (scan) scan->stop();
}

void PersonRadar::tick() {
  uint32_t now = millis();

  // 1) drena a fila das callbacks para os maps (só aqui os maps são tocados)
  while (true) {
    MacEvent e;
    portENTER_CRITICAL(&_qMux);
    if (_qTail == _qHead) { portEXIT_CRITICAL(&_qMux); break; }
    e.mac = _q[_qTail].mac;
    e.ble = _q[_qTail].ble;
    _qTail = (uint16_t)((_qTail + 1) % Q_SIZE);
    portEXIT_CRITICAL(&_qMux);
    if (e.ble) _bleMacs[e.mac] = now;
    else       _wifiMacs[e.mac] = now;
  }

  // 2) decay: expira MACs inativos (a cada ~2s)
  static uint32_t lastDecay = 0;
  if (now - lastDecay < 2000) return;
  lastDecay = now;

  const uint32_t TIMEOUT_MS = 120000;  // 2 min
  for (auto it = _wifiMacs.begin(); it != _wifiMacs.end(); ) {
    if (now - it->second > TIMEOUT_MS) it = _wifiMacs.erase(it);
    else ++it;
  }
  for (auto it = _bleMacs.begin(); it != _bleMacs.end(); ) {
    if (now - it->second > TIMEOUT_MS) it = _bleMacs.erase(it);
    else ++it;
  }
}

// =================================================================== desenho ==
void PersonRadar::draw(TFT_eSPI &tft, bool full) {
  if (full) {
    tft.fillScreen(COLOR_BG);
    tft.fillRect(0, 0, tft.width(), 22, COLOR_SEL);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_FG, COLOR_SEL);
    tft.drawString("Radar de Pessoas", 6, 4, 2);
    tft.setTextColor(COLOR_FG, COLOR_BG);
    tft.drawString("Sniff passivo Wi-Fi + BLE. BACK=sair", 6, 224, 1);
  }

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COLOR_FG, COLOR_BG);

  int w = wifiCount(), b = bleCount(), t = totalCount();

  // --- gauge de lotacao ---
  int gx = 10, gy = 40, gw = 300, gh = 60;
  int pct = constrain(t * 100 / 30, 0, 100);  // max ~30 dispositivos
  uint16_t gaugeColor = (pct < 33) ? COLOR_OK : (pct < 66) ? COLOR_ACCENT : COLOR_WARN;

  tft.drawRect(gx, gy, gw, gh, COLOR_FG);
  tft.fillRect(gx + 1, gy + 1, gw - 2, gh - 2, COLOR_BG);
  int fillW = (gw - 2) * pct / 100;
  tft.fillRect(gx + 1, gy + 1, fillW, gh - 2, gaugeColor);

  char b_str[32];
  snprintf(b_str, sizeof(b_str), "%d dispositivos", t);
  tft.setTextColor(COLOR_ACCENT, COLOR_BG);
  tft.drawString(b_str, gx + 8, gy + 18, 6);

  // --- detalhes ---
  tft.setTextColor(COLOR_FG, COLOR_BG);
  snprintf(b_str, sizeof(b_str), "WiFi (probe reqs): %-2d", w);
  tft.drawString(b_str, 10, 120, 4);
  snprintf(b_str, sizeof(b_str), "BLE (anuncios):    %-2d", b);
  tft.drawString(b_str, 10, 150, 4);

  // --- interpretacao visual ---
  const char *vibe = "";
  if (t == 0)      vibe = "Ambiente vazio... quietao";
  else if (t <= 3) vibe = "Poucos dispositivos - calmo";
  else if (t <= 8) vibe = "Normalmente cheio - animado";
  else if (t <= 15) vibe = "Bem cheio - festa/evento";
  else             vibe = "SUPERLOTADO!!!";

  tft.setTextColor(COLOR_ACCENT, COLOR_BG);
  tft.drawString(vibe, 10, 190, 2);
}
