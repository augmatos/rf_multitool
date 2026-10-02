#pragma once
// ============================================================================
//  Achievements  -  troféus salvos na flash (NVS), desbloqueados usando os
//  outros apps do multitool. Casa com o RF Tamagotchi: cada conquista nova
//  dá um bônus de XP pro bichinho (aplicado na próxima vez que ele for aberto).
//
//  A classe NÃO acessa hardware de rádio: os outros apps chamam os report*()
//  quando algo relevante acontece, e main.cpp chama tick() sempre (pra somar
//  o uptime total entre boots).
// ============================================================================
#include <Arduino.h>
#include <TFT_eSPI.h>

#define ACH_COUNT 11

class Achievements {
public:
  void begin();                          // carrega da NVS
  void tick();                           // acumula uptime; chamar sempre no loop()

  void reportWifiScan(int netsFound);    // soma "avistamentos" de redes (nao unicas)
  void reportSweepUsed();
  void reportSubGhzCaptureUsed();
  void reportBleScanUsed();
  void reportTamagotchiLevel(int level);

  int  drainXpBonus();                   // devolve o XP pendente e zera (consome uma vez)

  void selMove(int d);                   // navega a lista (encoder)
  void draw(TFT_eSPI &tft, bool full);

private:
  struct Ach { const char *name; const char *desc; bool unlocked; };
  Ach _list[ACH_COUNT];
  int _sel = 0;

  uint32_t _wifiNetsTotal  = 0;    // total cumulativo de redes "vistas" em scans
  uint32_t _uptimeSecTotal = 0;    // uptime cumulativo entre boots
  uint32_t _sessionStartMs = 0;
  uint32_t _lastSaveMs     = 0;
  uint32_t _lastUnlockMs   = 0;
  const char *_lastUnlocked = "";

  int _pendingXp = 0;

  void unlock(int idx);
  void save();
  String progressText(int idx) const;
};

extern Achievements ach;
