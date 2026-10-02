#pragma once
// ============================================================================
//  esphishing  -  ESP32-S3 RF multitool : MAPA DE PINOS
//  Placa: ESP32-S3-DevKitC-1 (modulo WROOM-1, ex.: N16R8)
//
//  DOIS barramentos SPI independentes:
//    - DISPLAY -> SPI3 / HSPI  (TFT_eSPI, pinos definidos no platformio.ini)
//    - RADIOS  -> SPI2 / FSPI  (objeto global "SPI", compartilhado por
//                               CC1101 + NRF24 #1 + NRF24 #2)
//
//  TUDO AQUI E 3.3V. O CC1101 e o NRF24 NAO sao tolerantes a 5V.
// ============================================================================

// ---------- DISPLAY (ILI9341, SPI3/HSPI) ------------------------------------
//  Espelham as flags TFT_* do platformio.ini (aqui so para a documentacao).
#define DISP_SCLK   12
#define DISP_MOSI   11
#define DISP_MISO   13
#define DISP_CS     10
#define DISP_DC      9
#define DISP_RST    14
#define DISP_BL     21   // backlight (pino com PWM)

// ---------- BARRAMENTO SPI DOS RADIOS (SPI2/FSPI, objeto global "SPI") -------
#define RAD_SCK     18
#define RAD_MOSI    17
#define RAD_MISO     8

// CC1101 (sub-GHz)
#define CC1101_CS    7
#define CC1101_GDO0  6   // IRQ de sync/pacote do CC1101

// NRF24L01+PA+LNA  #1
#define NRF1_CE      4
#define NRF1_CSN     2

// NRF24L01+PA+LNA  #2
#define NRF2_CE     42
#define NRF2_CSN    41

// ---------- ENCODER ROTATIVO (KY-040) ---------------------------------------
#define ENC_CLK     38   // "CLK" / fase A
#define ENC_DT      39   // "DT"  / fase B
#define ENC_SW      47   // botao do eixo (ativo em LOW)

// ---------- BOTOES (ativo LOW, INPUT_PULLUP) --------------------------------
#define BTN_OK       5   // confirma / entra
#define BTN_BACK     1   // volta / sai do app
#define BTN_AUX     40   // acao de contexto

// ---------- LEDs (ativo HIGH, com resistor de 220-330R em serie) ------------
#define LED_STAT    15   // status / heartbeat
#define LED_RF      16   // atividade de RF

// ---------- NOTAS -----------------------------------------------------------
//  * Pinos EVITADOS no ESP32-S3-WROOM-1: 0,3,19,20 (USB), 26-37 (flash/PSRAM),
//    45,46 (strapping), 48 (LED RGB). Nao mova sinais para esses pinos.
//  * Se o seu clone tiver o LED RGB no GPIO38, o encoder pode "pular":
//    mova ENC_CLK para o GPIO19 e grave pela porta UART.
