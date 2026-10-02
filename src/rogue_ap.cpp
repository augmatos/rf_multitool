#include "rogue_ap.h"
#include "config.h"
#include <WiFi.h>

bool RogueAp::isRunning() const { return _state == ST_RUNNING; }

void RogueAp::begin() {
  stopAp();
  _state = ST_WARNING;
  _sel = 0;
  _targetSsid = "";
}

void RogueAp::doScan() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  _scanCount = WiFi.scanNetworks();
  _sel = 0;
  _state = (_scanCount > 0) ? ST_SELECT : ST_NONE_FOUND;
}

void RogueAp::selMove(int d) {
  if (_state != ST_SELECT || _scanCount <= 0) return;
  _sel = ((_sel + d) % _scanCount + _scanCount) % _scanCount;
}

void RogueAp::confirmStep() {
  switch (_state) {
    case ST_WARNING:
      _state = ST_SCANNING;
      doScan();
      break;
    case ST_NONE_FOUND:
      doScan();      // tenta escanear de novo
      break;
    case ST_SELECT:
      _targetSsid    = WiFi.SSID(_sel);
      _targetChannel = WiFi.channel(_sel);
      WiFi.scanDelete();
      _state = ST_CONFIRM;
      break;
    case ST_CONFIRM:
      startAp();
      _state = ST_RUNNING;
      break;
    default:
      break;
  }
}

void RogueAp::startAp() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(_targetSsid.c_str(), nullptr, _targetChannel);   // rede ABERTA, mesmo SSID/canal
}

void RogueAp::stopAp() {
  if (_state == ST_RUNNING) WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
}

void RogueAp::cancel() {
  stopAp();
}

// =================================================================== desenho ==
static void warnBanner(TFT_eSPI &tft, int y) {
  tft.fillRect(0, y, tft.width(), 16, COLOR_WARN);
  tft.setTextColor(COLOR_BG, COLOR_WARN);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("SO PENTEST AUTORIZADO POR ESCRITO", tft.width() / 2, y + 8, 1);
  tft.setTextDatum(TL_DATUM);
}

void RogueAp::draw(TFT_eSPI &tft, bool full) {
  if (full) {
    tft.fillScreen(COLOR_BG);
    tft.fillRect(0, 0, tft.width(), 22, COLOR_SEL);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_FG, COLOR_SEL);
    tft.drawString("Rogue AP / Evil Twin", 6, 4, 2);
  }
  tft.fillRect(0, 24, tft.width(), tft.height() - 24, COLOR_BG);
  tft.setTextDatum(TL_DATUM);
  warnBanner(tft, 26);

  switch (_state) {
    case ST_WARNING: {
      tft.setTextColor(COLOR_FG, COLOR_BG);
      tft.drawString("Este modo clona o SSID de uma rede", 8, 54, 2);
      tft.drawString("ja transmitindo por perto, para medir", 8, 74, 2);
      tft.drawString("quantos dispositivos entram sozinhos.", 8, 94, 2);
      tft.setTextColor(COLOR_WARN, COLOR_BG);
      tft.drawString("Use SOMENTE com autorizacao por", 8, 122, 2);
      tft.drawString("escrito (engajamento de pentest).", 8, 142, 2);
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
      tft.drawString("Confirma clonar esta rede?", 8, 54, 2);
      tft.setTextColor(COLOR_ACCENT, COLOR_BG);
      char line[40];
      snprintf(line, sizeof(line), "\"%s\" (canal %d)", _targetSsid.c_str(), _targetChannel);
      tft.drawString(line, 8, 80, 2);
      tft.setTextColor(COLOR_FG, COLOR_BG);
      tft.drawString("So se voce tem autorizacao por", 8, 116, 2);
      tft.drawString("escrito para testar ESTA rede.", 8, 136, 2);
      tft.setTextColor(COLOR_OK, COLOR_BG);
      tft.drawString("[OK] confirmar e iniciar", 8, 176, 2);
      tft.setTextColor(COLOR_FG, COLOR_BG);
      tft.drawString("[BACK] cancelar e sair", 8, 200, 2);
      break;
    }
    case ST_RUNNING: {
      tft.setTextColor(COLOR_WARN, COLOR_BG);
      tft.drawString("EVIL TWIN ATIVO", 8, 54, 4);
      tft.setTextColor(COLOR_ACCENT, COLOR_BG);
      char line[40];
      snprintf(line, sizeof(line), "SSID: %s", _targetSsid.c_str());
      tft.drawString(line, 8, 88, 2);
      snprintf(line, sizeof(line), "Canal: %d  (rede ABERTA)", _targetChannel);
      tft.drawString(line, 8, 108, 2);

      tft.setTextColor(COLOR_FG, COLOR_BG);
      int clients = WiFi.softAPgetStationNum();
      snprintf(line, sizeof(line), "Dispositivos conectados: %d", clients);
      tft.drawString(line, 8, 140, 2);
      tft.setTextColor(COLOR_WARN, COLOR_BG);
      tft.drawString("Registre isso no relatorio do teste.", 8, 160, 1);

      tft.setTextColor(COLOR_FG, COLOR_BG);
      tft.drawString("[BACK] parar e sair", 8, 200, 2);
      break;
    }
  }
}
