// TESTE 4 - NRF24L01+PA+LNA (x2)  -> pio run -e test_nrf24 -t upload -t monitor
// Esperado: "NRF #1: OK" e "NRF #2: OK", depois uma linha de ocupacao 2.4GHz.
//           Pontos '.' = canal livre; digitos/'#' = atividade (WiFi/BLE).
// IMPORTANTE: rosqueie as ANTENAS antes de ligar (PA pode danificar sem antena)
//             e alimente os NRF com 3V3 ESTAVEL + capacitor (veja docs).
//           "FALHA" => quase sempre e ALIMENTACAO: adicione cap de 10uF no VCC
//           e use um regulador 3V3 dedicado.
#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include "pins.h"
#include "config.h"

RF24 n1(NRF1_CE, NRF1_CSN);
RF24 n2(NRF2_CE, NRF2_CSN);

void test(RF24 &r, const char *name) {
  // O primeiro begin() logo apos energizar pode falhar (rádio ainda nao pronto).
  // Tenta algumas vezes antes de declarar FALHA.
  bool ok = false;
  for (int i = 0; i < 5 && !ok; i++) {
    ok = r.begin(&SPI) && r.isChipConnected();
    if (!ok) delay(100);
  }
  Serial.printf("%s: %s\n", name, ok ? "OK" : "FALHA");
}

void setup() {
  Serial.begin(115200);
  delay(400);
  SPI.begin(RAD_SCK, RAD_MISO, RAD_MOSI);

  // Tira o CC1101 do barramento compartilhado. Com o CS dele solto, o CC1101
  // pode ficar "selecionado" e dirigir o MISO, colidindo com os NRF -> os dois
  // "somem" do bus. Deseleciona (CS = HIGH) antes de falar com os NRF.
  pinMode(CC1101_CS, OUTPUT);
  digitalWrite(CC1101_CS, HIGH);

  // Deseleciona AMBOS os NRF antes de sondar qualquer um: com o CSN do outro
  // modulo solto, ele dirige o MISO e derruba a deteccao (o #1 so passava
  // DEPOIS que o #2 era inicializado). CSN = HIGH -> deselecionado.
  pinMode(NRF1_CSN, OUTPUT); digitalWrite(NRF1_CSN, HIGH);
  pinMode(NRF2_CSN, OUTPUT); digitalWrite(NRF2_CSN, HIGH);

  // Pequena espera pos-energizacao (NRF bom estabiliza em ~100 ms).
  Serial.println("Aguardando NRF estabilizar...");
  delay(300);
  test(n1, "NRF #1");
  test(n2, "NRF #2");

  n1.setAutoAck(false);
  n1.disableCRC();
  n1.setDataRate(RF24_1MBPS);
  n1.startListening();
  n1.stopListening();
  Serial.println("Scan de ocupacao 2.4GHz (NRF #1):");
}

void loop() {
  char line[NRF_NUM_CHANNELS + 1];
  for (int ch = 0; ch < NRF_NUM_CHANNELS; ch++) {
    int c = 0;
    for (int r = 0; r < 60; r++) {
      n1.setChannel(ch);
      n1.startListening();
      delayMicroseconds(NRF_DWELL_US);
      n1.stopListening();
      if (n1.testRPD()) c++;
    }
    line[ch] = (c == 0) ? '.' : (c < 10 ? char('0' + c) : '#');
  }
  line[NRF_NUM_CHANNELS] = 0;
  Serial.println(line);
  delay(200);
}
