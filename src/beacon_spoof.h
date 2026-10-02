#pragma once
// ============================================================================
//  Beacon Spoofing  -  clona SSID + BSSID reais de um AP existente.
//  Transmite beacons identicos ao AP alvo (mesmo SSID, mesmo BSSID, mesmo canal).
//
//  ⚠️  ISTO E PENTEST AUTORIZADO: simula um AP identico ao real pra testar
//      clientes (eles se conectam ao primeiro que acham com aquele SSID?).
//      Use SOMENTE em redes suas ou com autorizacao por escrito.
//
//  Modos:
//   - Continuo: envia beacons o tempo todo (clone "sempre la").
//   - Burst: um clique = uma rajada so (mais controlado).
//  Com cooldown entre rajadas e confirmacao na UI.
// ============================================================================
#include <Arduino.h>
#include <TFT_eSPI.h>

#define BEACON_SPOOF_MAX_NETS 20

class BeaconSpoof {
public:
  void begin();                          // faz scan inicial

  void  scanNets();                      // rescanneia as redes (pode levar alguns segundos)
  int   netCount() const { return _netCount; }
  void  selMove(int d);                  // navega entre as redes capturadas
  int   selIdx() const { return _selIdx; }

  void  setEnabled(bool on) { _enabled = on; }
  bool  enabled() const { return _enabled; }

  void  tick();                          // envia uma rajada continua (modo continuo)
  bool  burstOnHeld(bool okHeld);        // envia rajada so se OK seguro + cooldown; devolve true se enviou

  void  draw(TFT_eSPI &tft, bool full);

private:
  struct Network {
    char   ssid[33];        // max 32 chars + null
    uint8_t bssid[6];
    uint8_t channel;
    int    rssi;
  };
  Network _net[BEACON_SPOOF_MAX_NETS];
  int     _netCount  = 0;
  int     _selIdx    = 0;
  bool    _enabled   = false;
  uint32_t _lastBurstMs = 0;

  uint32_t _sent = 0;
  uint8_t  _channel = 1;

  int  buildBeacon(uint8_t *pkt, const char *ssid, const uint8_t *bssid, uint8_t ch);
  void sendBurst();
};
