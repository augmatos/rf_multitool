#pragma once
// ============================================================================
//  Radar de pessoas  -  contador passivo de dispositivos Wi-Fi + BLE por perto.
//  - Captura probe requests (clientes procurando redes) + anúncios BLE.
//  - Conta MACs únicos, que expiram por inatividade (não persistem).
//  - Mostra um medidor de "lotação" (sem identificar ninguém).
//
//  Modo sniff / promíscuo. Apenas recebe, não transmite.
//
//  Concorrência: as callbacks Wi-Fi/BLE rodam em OUTRAS tasks. Elas NÃO tocam
//  nos std::map daqui -- só empilham o MAC numa fila protegida por spinlock
//  (ver radar.cpp). O tick(), no loop principal, é o único que mexe nos maps.
// ============================================================================
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <map>

class PersonRadar {
public:
  void begin();                     // ativa promiscuous mode + BLE scan (não bloqueante)
  void end();                       // desliga promíscuo + BLE (chamar ao sair do app)
  void tick();                      // drena a fila e faz o decay -- só no loop principal
  void draw(TFT_eSPI &tft, bool full);

  int wifiCount() const  { return (int)_wifiMacs.size(); }
  int bleCount() const   { return (int)_bleMacs.size(); }
  int totalCount() const { return wifiCount() + bleCount(); }

private:
  // MAC (6 bytes) -> last_seen_ms. Mexidos SOMENTE em tick() (loop principal).
  std::map<uint64_t, uint32_t> _wifiMacs;
  std::map<uint64_t, uint32_t> _bleMacs;
};

extern PersonRadar radar;
