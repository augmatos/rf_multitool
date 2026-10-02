// TESTE 2 - Encoder KY-040 + botoes  -> pio run -e test_encoder -t upload -t monitor
// Esperado no monitor serial: a contagem "enc" muda ao girar; SW/OK/BACK/AUX
//           vao para 1 ao apertar. Se a contagem for ao contrario, inverta
//           ENC_DT/ENC_CLK no attachHalfQuad. Se pular de 2 em 2, ajuste
//           ENC_STEPS_PER_DETENT no config.h.
#include <Arduino.h>
#include <ESP32Encoder.h>
#include "pins.h"
#include "config.h"

ESP32Encoder enc;

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(ENC_SW, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  pinMode(BTN_AUX, INPUT_PULLUP);
  ESP32Encoder::useInternalWeakPullResistors = puType::up;   // se nao compilar: puType::UP
  enc.attachHalfQuad(ENC_DT, ENC_CLK);
  enc.clearCount();
  Serial.println("Gire o encoder e aperte os botoes...");
}

void loop() {
  long detents = (long)enc.getCount() / ENC_STEPS_PER_DETENT;
  Serial.printf("enc=%ld  SW=%d OK=%d BACK=%d AUX=%d\n",
                detents,
                !digitalRead(ENC_SW),
                !digitalRead(BTN_OK),
                !digitalRead(BTN_BACK),
                !digitalRead(BTN_AUX));
  delay(150);
}
