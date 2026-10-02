#pragma once
// ============================================================================
//  Sub-GHz Capture & Replay  -  grava e reproduz o SEU controle RF de codigo
//  fixo (portao/tomada estilo PT2262 / EV1527).
//
//  SO funciona com codigo FIXO. Rolling code (a maioria dos carros e
//  portoes comerciais modernos) troca o codigo a cada acionamento -- uma
//  gravacao simples NUNCA vai destrancar. Use somente no SEU equipamento.
//
//  Tecnica: CC1101 em modo assincrono "serial" (OOK/ASK cru, sem pacote/CRC).
//  O GDO0 vira um logic-analyzer de 1 bit: uma interrupcao de borda grava a
//  duracao de cada pulso (RX) e o mesmo pino e "batido" em modo digital pra
//  reproduzir o trem de pulsos gravado (TX). 3 slots persistem na NVS
//  (Preferences), igual ao padrao usado em tamagotchi.cpp.
//
//  Ruido ambiente em 433/315/390/868 MHz pode "sujar" uma captura (o GDO0
//  em modo async fica sempre demodulando, nao ha squelch). Capture perto do
//  controle e aperte o botao dele durante a janela de gravacao.
//
//  A classe NAO trata botoes; o main.cpp decide quando capturar/reproduzir/
//  trocar slot ou frequencia, e desenha via draw().
// ============================================================================
#include <Arduino.h>
#include <TFT_eSPI.h>

#define SUBCAP_NUM_SLOTS   3
#define SUBCAP_MAX_PULSES  800     // edges (varios repeats de um codigo fixo de ~24 bits)
#define SUBCAP_MIN_PULSES  8       // minimo de edges pra considerar a captura valida
#define SUBCAP_TIMEOUT_MS  5000    // janela maxima de captura
#define SUBCAP_IDLE_GAP_US 15000   // silencio > isso = fim da rajada, encerra a captura cedo

class SubGhzCapture {
public:
  void begin();                        // abre a NVS, carrega os 3 slots, arma RX
  void startCapture();                 // arma a captura no slot atual
  void cancelCapture();                // desarma a interrupcao (sair do app / trocar config)
  void tick();                         // chamar sempre; fecha a captura por timeout/silencio
  void play();                         // reproduz o slot atual (TX, bloqueante)
  void nextSlot(int d);                // troca slot -1/+1 (com wrap)
  void nextFreq();                     // cicla pelos presets de frequencia de captura
  void save();                         // persiste os 3 slots na NVS
  void draw(TFT_eSPI &tft, bool full);

  bool isCapturing() const { return _capturing; }

  static const int NUM_FREQS = 4;

private:
  struct Slot {
    float    freqMHz = 0;               // frequencia em que este slot foi GRAVADO
    uint16_t count   = 0;                // numero de duracoes validas em pulses[]
    uint16_t pulses[SUBCAP_MAX_PULSES] = {};   // duracoes em microssegundos
  };

  Slot _slots[SUBCAP_NUM_SLOTS];
  int  _slotIdx = 0;
  int  _freqIdx = 0;                    // preset de frequencia usado na PROXIMA captura

  bool        _capturing      = false;
  uint32_t    _captureStartMs = 0;
  const char *_status  = "";
  uint32_t    _lastReact = 0;

  // ---- preenchidos pela ISR (subcapIsr), lidos so depois de detachInterrupt ----
  volatile uint16_t _capBuf[SUBCAP_MAX_PULSES];
  volatile uint16_t _capCount     = 0;
  volatile uint32_t _capLastEdgeUs = 0;
  volatile bool     _capHaveEdge   = false;

  void armRx();                        // (re)configura o CC1101 em OOK assincrono + SetRx
  void finishCapture();
  void loadSlot(int i);
  void saveSlot(int i);

  friend void subcapIsr();
};

extern SubGhzCapture subCapture;
