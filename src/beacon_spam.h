#pragma once
// ============================================================================
//  Beacon Spammer  -  cria varias redes Wi-Fi "fantasma" com nomes de brincadeira.
//  Monta frames 802.11 beacon crus e transmite com esp_wifi_80211_tx().
//
//  ⚠️  ISTO INTERFERE NA LISTA DE Wi-Fi DE QUEM ESTA POR PERTO e e ilegal em
//      alguns lugares. Use so em laboratorio / redes suas / com autorizacao.
//
//  Modos:
//   - Engracado / Historia / Picante: spam continuo com nomes de piada
//     (demo/awareness). Picante = nomes espertos/instigantes, sem conteudo
//     explicito -- isso transmite p/ QUALQUER dispositivo por perto, sem
//     consentimento, entao o tom fica malicioso mas nunca vulgar.
//   - Stress-test: modo de PENTEST AUTORIZADO para estressar UM aparelho alvo.
//     Deliberadamente LIMITADO (nao e um "flood" irrestrito): numero de SSIDs
//     por rajada com teto, cooldown entre rajadas, liga/desliga por clique
//     (toggle -- NAO exige mais segurar o botao, decisao consciente do dono
//     do dispositivo), e exige confirmar autorizacao toda vez que entra no
//     modo. Trocar de modo ou sair do app sempre desliga a transmissao.
//
//  A classe NAO trata botoes; o main.cpp decide os eventos e chama draw().
// ============================================================================
#include <Arduino.h>
#include <TFT_eSPI.h>

class BeaconSpam {
public:
  void begin();                          // gera BSSID base, zera contadores
  void setEnabled(bool on) { _enabled = on; }
  bool enabled() const { return _enabled; }
  void cycleMode();                      // Engracado -> Historia -> Picante -> Stress-test -> ...
  void scrollPreview(int d);             // rola a previa da lista (modos continuos)
  void tick();                           // envia uma rajada continua (modos Engracado/Historia)
  void draw(TFT_eSPI &tft, bool full);

  // ---- stress-test (pentest, 1 alvo autorizado) ----
  bool isStress() const;
  bool stressAuthorized() const;
  void authorizeStress();                // marca autorizado (apos confirmacao na UI)
  void adjustStressCount(int d);         // ajusta nº de SSIDs por rajada (1..teto)
  bool stressBurst(bool active);         // so transmite se autorizado + active (toggle) + cooldown; devolve true se enviou

private:
  enum Mode { M_FUN, M_STORY, M_HOT, M_STRESS };

  bool     _enabled = false;
  Mode     _mode    = M_FUN;
  int      _scroll  = 0;
  int      _chIdx   = 0;
  uint8_t  _channel = 1;
  uint32_t _sent    = 0;
  uint8_t  _bssidBase[6];

  // stress-test
  int      _stressCount      = 20;
  bool     _stressAuthorized = false;
  uint32_t _lastBurstMs      = 0;

  // controle de redesenho (evita flicker: so limpa o corpo em transicao)
  Mode _lastDrawMode = M_FUN;
  bool _lastDrawAuth = false;
  bool _firstDraw    = true;

  int         listCount() const;
  const char *listAt(int i) const;
  void        makeBssid(uint8_t *mac, int idx, int chSlot);
  int         buildBeacon(uint8_t *pkt, const char *ssid, const uint8_t *bssid, uint8_t ch);
  void        sendBurst(int count);
};
