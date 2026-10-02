// TESTE 5 - WiFi (radio interno do ESP32-S3)  -> pio run -e test_wifi -t upload -t monitor
// Esperado: lista das redes ao redor a cada ~3s (RSSI, canal, OPEN/SEC, SSID).
// Nao precisa de modulo externo: valida o radio interno e o ambiente de build.
#include <Arduino.h>
#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(200);
  Serial.println("WiFi scanner iniciado");
}

void loop() {
  int n = WiFi.scanNetworks();
  Serial.printf("== %d redes ==\n", n);
  for (int i = 0; i < n; i++) {
    Serial.printf("%2d) %4d dBm  ch%2d  %-4s  %s\n",
                  i, WiFi.RSSI(i), WiFi.channel(i),
                  (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "OPEN" : "SEC",
                  WiFi.SSID(i).c_str());
  }
  Serial.println();
  delay(3000);
}
