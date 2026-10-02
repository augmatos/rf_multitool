#include "beacon_spam.h"
#include "config.h"
#include <WiFi.h>
#include "esp_wifi.h"

// ---- limites do stress-test ----
#define STRESS_MAX_SSIDS   75     // teto de SSIDs por rajada
// Cooldown ZERADO por escolha explicita e consciente do dono do hardware, pra
// testar o limite fisico de transmissao do ESP32-S3 (~40 rajadas/s, ritmo do
// loop principal em main.cpp). Isso remove a trava de taxa que limitava o
// stress-test a ~3 rajadas/s -- sem ela, a taxa de frames sobe ~12x (para
// ~3000 frames/s com STRESS_MAX_SSIDS=75). Ver feedback-features-ofensivas-
// rf-multitool na memoria do projeto para o registro dessa decisao.
#define STRESS_COOLDOWN_MS 0

// ---- listas de SSIDs (sem acento: a fonte do TFT nao desenha á/ã/ç) ----
static const char *LIST_FUN[] = {
  "Vai_Cair_Nessa_Rede?",
  "FBI_Surveillance_Van_04",
  "Wi-Fi_da_Vizinha_NAO",
  "Carregando...",
  "Conectando...",
  "Senha_eh_12345",
  "Boleto_Vence_Hoje",
  "Tela_Azul.exe",
  "Definitivamente_Nao_Eh_Virus",
  "Click_Aqui_Mae",
  "Pinga_aqui_pra_internet",
  "Modem_Bugado",
  "Roteador_do_Ze",
  "Internet_Gratis_(mentira)",
  "NSA_Node_7",
  "Nao_Tem_Senha_Nao",
  "Vovo_Nao_Mexe_Aqui",
  "Escondido_kkkk",
  "Pare_De_Olhar_Minha_Rede",
  "5G_Espalha_Boato",
  "HUE_HUE_HUE_BR",
  "Rede_Fantasma_666",
  "Toma_Beacon",
  "Wifi_Pago_no_Pix"
};
static const char *LIST_STORY[] = {
  "== Voce foi ==",
  "== marcado por ==",
  "== um teste de ==",
  "== seguranca ==",
  "== nunca use ==",
  "== WiFi aberto ==",
  "== fica a dica ==",
  "== :) =="
};
// picante: espertos/instigantes, NUNCA explicitos -- isso aparece na lista de
// Wi-Fi de qualquer um por perto, sem escolha da pessoa
static const char *LIST_HOT[] = {
  "Rede_Proibida_+18",
  "Curioso(a)?_Nao_Entra",
  "So_Pra_Maiores_kk",
  "Rede_Picante_Cuidado",
  "Zona_Perigosa_kkkk",
  "Quente_Igual_Pimenta",
  "Entra_Se_For_Corajoso(a)",
  "Sextou_e_a_Rede_Tambem",
  "Nao_Sou_Flertando_kk",
  "Rede_do_Tio_Suspeito",
  "Acesso_Restrito_18+",
  "Pensa_Bem_Antes_de_Entrar",
  "Ninguem_Vai_Saber_kkkk",
  "Rede_Naughty_BR"
};

int BeaconSpam::listCount() const {
  switch (_mode) {
    case M_FUN:   return (int)(sizeof(LIST_FUN)   / sizeof(char *));
    case M_STORY: return (int)(sizeof(LIST_STORY) / sizeof(char *));
    case M_HOT:   return (int)(sizeof(LIST_HOT)    / sizeof(char *));
    default:      return _stressCount;   // M_STRESS: nomes gerados, claramente de teste
  }
}
const char *BeaconSpam::listAt(int i) const {
  if (_mode == M_STRESS) {
    static char buf[20];
    snprintf(buf, sizeof(buf), "RF-TEST-%02d", i);   // nome neutro, nao enganoso
    return buf;
  }
  if (_mode == M_HOT) return LIST_HOT[i];
  return (_mode == M_STORY) ? LIST_STORY[i] : LIST_FUN[i];
}

bool BeaconSpam::isStress() const         { return _mode == M_STRESS; }
bool BeaconSpam::stressAuthorized() const { return _stressAuthorized; }
void BeaconSpam::authorizeStress()        { _stressAuthorized = true; }
void BeaconSpam::adjustStressCount(int d) { _stressCount = constrain(_stressCount + d, 1, STRESS_MAX_SSIDS); }

void BeaconSpam::begin() {
  randomSeed(micros());
  for (int i = 0; i < 6; i++) _bssidBase[i] = random(256);
  _bssidBase[0] = 0x02;              // MAC localmente administrado, unicast
  _sent = 0; _chIdx = 0; _channel = 1; _scroll = 0;
  _mode = M_FUN; _enabled = false; _stressAuthorized = false;
  _firstDraw = true;
}

void BeaconSpam::cycleMode() {
  _mode = (Mode)((_mode + 1) % 4);
  _scroll = 0;
  if (_mode == M_STRESS) { _enabled = false; _stressAuthorized = false; }  // entra travado
}

void BeaconSpam::scrollPreview(int d) {
  int n = listCount();
  _scroll = ((_scroll + d) % n + n) % n;
}

// BSSID estavel por SSID+canal (parece um AP de verdade que nao muda de MAC
// NEM de canal -- sem o "+ chSlot", o MESMO BSSID anunciava canais diferentes
// a cada hop, e o SO do celular/notebook tratava isso como inconsistente,
// fazendo a rede "piscar" na lista de Wi-Fi em vez de ficar estavel)
void BeaconSpam::makeBssid(uint8_t *mac, int idx, int chSlot) {
  mac[0] = 0x02;
  mac[1] = _bssidBase[1];
  mac[2] = _bssidBase[2];
  mac[3] = _bssidBase[3];
  mac[4] = _bssidBase[4];
  // espacamento entre blocos de canal tem que ser > maior lista possivel
  // (STRESS_MAX_SSIDS), senao dois idx de canais diferentes colidem no mesmo
  // byte e voltam a ter o problema de "BSSID mudando de canal" que gerava o
  // piscar da rede na lista de Wi-Fi
  mac[5] = (uint8_t)(0x10 + idx + chSlot * 80);
}

// Monta um frame beacon 802.11 e devolve o tamanho
int BeaconSpam::buildBeacon(uint8_t *pkt, const char *ssid, const uint8_t *bssid, uint8_t ch) {
  int i = 0;
  // --- cabecalho MAC (24 bytes) ---
  pkt[i++] = 0x80; pkt[i++] = 0x00;                 // frame control: beacon
  pkt[i++] = 0x00; pkt[i++] = 0x00;                 // duration
  for (int k = 0; k < 6; k++) pkt[i++] = 0xFF;      // destino: broadcast
  for (int k = 0; k < 6; k++) pkt[i++] = bssid[k];  // origem = BSSID
  for (int k = 0; k < 6; k++) pkt[i++] = bssid[k];  // BSSID
  pkt[i++] = 0x00; pkt[i++] = 0x00;                 // seq/frag

  // --- corpo fixo (12 bytes) ---
  for (int k = 0; k < 8; k++) pkt[i++] = 0x00;      // timestamp
  pkt[i++] = 0x64; pkt[i++] = 0x00;                 // beacon interval (100 TU)
  pkt[i++] = 0x01; pkt[i++] = 0x04;                 // capability info (ESS)

  // --- SSID ---
  int slen = strlen(ssid); if (slen > 32) slen = 32;
  pkt[i++] = 0x00; pkt[i++] = (uint8_t)slen;
  for (int k = 0; k < slen; k++) pkt[i++] = ssid[k];

  // --- taxas suportadas ---
  pkt[i++] = 0x01; pkt[i++] = 0x08;
  static const uint8_t rates[8] = {0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c};
  for (int k = 0; k < 8; k++) pkt[i++] = rates[k];

  // --- DS param (canal) ---
  pkt[i++] = 0x03; pkt[i++] = 0x01; pkt[i++] = ch;
  return i;
}

void BeaconSpam::sendBurst(int count) {
  static const uint8_t HOP[3] = {1, 6, 11};         // canais nao sobrepostos
  int chSlot = _chIdx;                              // slot usado NESTA rajada (0,1,2)
  _channel = HOP[chSlot];
  _chIdx = (_chIdx + 1) % 3;
  esp_wifi_set_channel(_channel, WIFI_SECOND_CHAN_NONE);

  uint8_t pkt[256];
  uint8_t mac[6];
  for (int i = 0; i < count; i++) {
    makeBssid(mac, i, chSlot);
    int len = buildBeacon(pkt, listAt(i), mac, _channel);
    esp_wifi_80211_tx(WIFI_IF_STA, pkt, len, false);
    _sent++;
  }
}

void BeaconSpam::tick() {
  sendBurst(listCount());   // modos continuos (Engracado/Historia)
}

bool BeaconSpam::stressBurst(bool active) {
  if (_mode != M_STRESS || !_stressAuthorized || !active) return false;
  if (millis() - _lastBurstMs < STRESS_COOLDOWN_MS) return false;   // respeita o cooldown
  _lastBurstMs = millis();
  sendBurst(_stressCount);
  return true;
}

// =================================================================== desenho ==
void BeaconSpam::draw(TFT_eSPI &tft, bool full) {
  // limpa o corpo so em transicao de layout (evita flicker; texto usa fundo opaco)
  bool relayout = full || _firstDraw || _mode != _lastDrawMode || _stressAuthorized != _lastDrawAuth;

  if (full) {
    tft.fillScreen(COLOR_BG);
    tft.fillRect(0, 0, tft.width(), 22, COLOR_SEL);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_FG, COLOR_SEL);
    tft.drawString("Beacon Spammer", 6, 4, 2);
  }
  if (relayout) tft.fillRect(0, 24, tft.width(), tft.height() - 24, COLOR_BG);

  tft.setTextDatum(TL_DATUM);
  char b[52];

  // ------------------------------------------------ modo STRESS-TEST ---------
  if (_mode == M_STRESS) {
    if (relayout) {
      tft.fillRect(0, 26, tft.width(), 16, COLOR_WARN);
      tft.setTextColor(COLOR_BG, COLOR_WARN);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("STRESS-TEST: 1 ALVO AUTORIZADO", tft.width() / 2, 34, 1);
      tft.setTextDatum(TL_DATUM);
    }

    if (!_stressAuthorized) {
      tft.setTextColor(COLOR_FG, COLOR_BG);
      tft.drawString("Teste de carga de beacons para", 8, 52, 2);
      tft.drawString("estressar UM aparelho que voce TEM", 8, 72, 2);
      tft.drawString("autorizacao por escrito p/ testar.", 8, 92, 2);
      tft.setTextColor(COLOR_WARN, COLOR_BG);
      tft.drawString("Ainda irradia p/ quem estiver perto:", 8, 118, 1);
      tft.drawString("fique perto do alvo e use por pouco tempo.", 8, 130, 1);
      tft.setTextColor(COLOR_OK, COLOR_BG);
      tft.drawString("[clique encoder] confirmo autorizacao", 8, 156, 2);
      tft.setTextColor(COLOR_FG, COLOR_BG);
      tft.drawString("[AUX] trocar modo   [BACK] sair", 8, 180, 2);
    } else {
      tft.setTextColor(COLOR_FG, COLOR_BG);
      snprintf(b, sizeof(b), "SSIDs por rajada: %-2d (max %d)", _stressCount, STRESS_MAX_SSIDS);
      tft.drawString(b, 8, 52, 2);
      snprintf(b, sizeof(b), "Frames enviados: %-10lu", (unsigned long)_sent);
      tft.drawString(b, 8, 76, 2);
      snprintf(b, sizeof(b), "Cooldown: %d ms   Canal: %-2d", STRESS_COOLDOWN_MS, _channel);
      tft.drawString(b, 8, 100, 2);

      // fonte 4 e proporcional -- limpa a faixa antes (mesmo motivo do bug do
      // LIGADO/DESLIGADO nos modos continuos: larguras diferentes em pixels)
      tft.fillRect(8, 138, 160, 28, COLOR_BG);
      tft.setTextColor(_enabled ? COLOR_OK : COLOR_FG, COLOR_BG);
      tft.drawString(_enabled ? "LIGADO" : "DESLIGADO", 8, 138, 4);

      tft.setTextColor(COLOR_WARN, COLOR_BG);
      tft.drawString("Clique = liga/desliga transmissao", 8, 172, 1);
      tft.setTextColor(COLOR_FG, COLOR_BG);
      tft.drawString("girar=nº SSIDs  AUX=modo  BACK=sair", 8, 188, 1);
    }
    _lastDrawMode = _mode; _lastDrawAuth = _stressAuthorized; _firstDraw = false;
    return;
  }

  // ------------------------------------------------ modos continuos ----------
  const char *modeName = (_mode == M_FUN) ? "Engracado" : (_mode == M_STORY) ? "Historia" : "Picante";

  // fonte tamanho 4 e proporcional (largura por letra varia) -- preencher com
  // espacos ate "N caracteres" nao garante a mesma largura em PIXELS entre
  // "LIGADO" e "DESLIGADO" (3 letras reais a mais), entao limpa a faixa antes
  // em vez de confiar no auto-apagar do drawString()
  tft.fillRect(8, 28, 160, 28, COLOR_BG);
  tft.setTextColor(_enabled ? COLOR_OK : COLOR_FG, COLOR_BG);
  tft.drawString(_enabled ? "LIGADO" : "DESLIGADO", 8, 28, 4);

  tft.setTextColor(COLOR_FG, COLOR_BG);
  snprintf(b, sizeof(b), "Frames: %-10lu", (unsigned long)_sent);
  tft.drawString(b, 8, 62, 4);

  snprintf(b, sizeof(b), "Canal: %-2d   Modo: %-9s", _channel, modeName);
  tft.drawString(b, 8, 96, 2);
  snprintf(b, sizeof(b), "SSIDs na lista: %-2d", listCount());
  tft.drawString(b, 8, 116, 2);

  int n = listCount();
  for (int r = 0; r < 3; r++) {
    int idx = (_scroll + r) % n;
    tft.setTextColor(COLOR_ACCENT, COLOR_BG);
    snprintf(b, sizeof(b), "%2d) %-30s", idx, listAt(idx));
    tft.drawString(b, 8, 138 + r * 15, 1);
  }
  tft.setTextColor(COLOR_WARN, COLOR_BG);
  tft.drawString("So em lab/redes suas - interfere em terceiros!", 6, 190, 1);
  tft.setTextColor(COLOR_FG, COLOR_BG);
  tft.drawString("clique=liga/desliga AUX=modo girar=lista BACK=sai", 6, 210, 1);

  _lastDrawMode = _mode; _lastDrawAuth = _stressAuthorized; _firstDraw = false;
}
