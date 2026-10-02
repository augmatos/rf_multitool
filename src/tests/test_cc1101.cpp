// TESTE 3 - CC1101 sub-GHz  ->  pio run -e test_cc1101 -t upload -t monitor
// Esperado: "CC1101: OK (detectado)" e o RSSI variando. Acione um controle
//           433MHz (portao/carro) por perto: o RSSI deve subir (ex.: de -90
//           para -40 dBm). "FALHA" => confira 3V3, GND, CS, MISO e o bus.
#include <Arduino.h>
#include <SPI.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include "pins.h"
#include "config.h"

void setup() {
  Serial.begin(115200);
  delay(400);
  SPI.begin(RAD_SCK, RAD_MISO, RAD_MOSI);
  ELECHOUSE_cc1101.setSpiPin(RAD_SCK, RAD_MISO, RAD_MOSI, CC1101_CS);

  if (ELECHOUSE_cc1101.getCC1101()) {
    Serial.println("CC1101: OK (detectado)");
  } else {
    Serial.println("CC1101: FALHA - verifique fiacao / 3V3 / bus SPI");
  }
  ELECHOUSE_cc1101.Init();
  ELECHOUSE_cc1101.setMHZ(SUBGHZ_DEFAULT_MHZ);
  ELECHOUSE_cc1101.SetRx();
}

void loop() {
  Serial.printf("%.2f MHz  RSSI = %d dBm\n",
                SUBGHZ_DEFAULT_MHZ, ELECHOUSE_cc1101.getRssi());
  delay(300);
}
