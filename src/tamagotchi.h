#pragma once
// ============================================================================
//  RF Tamagotchi  -  bichinho virtual que se alimenta de sinais de rádio.
//  - "Comida" = redes Wi-Fi + canais 2.4GHz ativos + sinal Sub-GHz + BLE
//    detectados no ambiente.
//  - Ganha XP, enche a barriga (fullness) e evolui de forma (7 estágios).
//  - Humor (happiness) depende de estar alimentado + carinho (encoder).
//  - Sem comida por muito tempo -> dorme. Barriga zerada por muito tempo ->
//    fica doente. Ambos revertem assim que voltar a receber sinal/comida.
//  - Estado persiste na flash (NVS) entre reinícios.
//
//  A classe NÃO mexe em hardware de rádio nem em botões: quem alimenta e
//  desenha é o main.cpp. Só depende do TFT para desenhar.
// ============================================================================
#include <Arduino.h>
#include <TFT_eSPI.h>

class Tamagotchi {
public:
  void begin();                              // carrega estado da NVS
  // subghzHit/bleDevices sao opcionais -- fontes extras de "comida" alem de WiFi/NRF
  void feed(int wifiNets, int nrfActiveCh, int subghzHit = 0, int bleDevices = 0);
  void petLove();                            // carinho -> humor sobe
  void addXp(int amount);                    // XP externo (ex.: bonus de achievement)
  void update();                             // decaimento por tempo (chamar sempre)
  void draw(TFT_eSPI &tft, bool full, bool showStats = false);  // full=true redesenha tudo
  void save();                               // grava estado na NVS
  int  level() const { return _level; }
  int  maxLevel() const { return _maxLevel; }
  int  evolutions() const { return _evolutions; }

private:
  // ---- estado persistente ----
  uint32_t _xp         = 0;
  int      _level      = 0;
  int      _fullness   = 60;   // 0..100 (barriga)
  int      _happiness  = 60;   // 0..100 (humor)
  uint32_t _ageSec     = 0;    // tempo de vida acumulado
  int      _maxLevel   = 0;    // recorde de nivel ja alcancado
  int      _evolutions = 0;    // quantas vezes trocou de estagio

  // ---- voláteis (UI/animação) ----
  int      _lastNets  = 0;
  int      _lastHits  = 0;
  int      _lastSub   = 0;
  int      _lastBle   = 0;
  int      _prevStage = -1;
  uint32_t _lastUpdateMs   = 0;
  uint32_t _fullAccumMs    = 0;
  uint32_t _happyAccumMs   = 0;
  uint32_t _ageAccumMs     = 0;
  uint32_t _eatMs          = 0;   // momento da última refeição (animação)
  uint32_t _reactMs        = 0;   // momento da última "fala"
  const char *_react       = "";  // texto flutuante atual
  uint32_t _lastFeedMs     = 0;   // ultima vez que feed() rendeu comida > 0 -> base do "dormindo"
  uint32_t _fullZeroSinceMs = 0;  // 0 = barriga nao esta zerada; senao, quando ZEROU -> base do "doente"

  int  xpForNext() const;     // XP necessário p/ subir de nível
  int  stage() const;         // 0..6 a partir do nível
  const char *stageName() const;
  void drawPet(TFT_eSPI &tft, int cx, int cy);
  void drawStats(TFT_eSPI &tft, bool full);
};
