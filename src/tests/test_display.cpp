// TESTE 1 - Display ILI9341  ->  pio run -e test_display -t upload -t monitor
// Esperado: tela limpa com "Display OK", 3 quadrados (vermelho/verde/azul)
//           e um contador subindo. Se as cores sairem trocadas, troque no
//           platformio.ini -DILI9341_2_DRIVER=1 por -DILI9341_DRIVER=1.
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "pins.h"
#include "config.h"

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  pinMode(DISP_BL, OUTPUT);
  digitalWrite(DISP_BL, HIGH);
  tft.init();
  tft.setRotation(UI_ROTATION);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Display OK", 10, 10, 4);
  tft.fillRect(10, 60, 60, 60, TFT_RED);
  tft.fillRect(80, 60, 60, 60, TFT_GREEN);
  tft.fillRect(150, 60, 60, 60, TFT_BLUE);
  Serial.printf("TFT %dx%d iniciado\n", tft.width(), tft.height());
}

void loop() {
  static int i = 0;
  tft.fillRect(10, 150, 200, 30, TFT_BLACK);
  tft.drawString("contador: " + String(i++), 10, 150, 4);
  delay(500);
}
