#pragma once
// ============================================================================
//  esphishing - parametros e flags do firmware
// ============================================================================

#define FW_NAME     "esphishing"
#define FW_VERSION  "0.1.0"

// --- Sub-GHz (CC1101). Bandas validas: 300-348 / 387-464 / 779-928 MHz ------
#define SUBGHZ_DEFAULT_MHZ   433.92f
#define SUBGHZ_SWEEP_START   433.00f
#define SUBGHZ_SWEEP_END     434.80f
#define SUBGHZ_SWEEP_STEP    0.05f
#define WATERFALL_STEP       0.02f   // passo mais fino, so pro Waterfall (mais colunas)

// --- WiFi Scanner -------------------------------------------------------
#define WIFI_SCAN_INTERVAL_MS 5000    // intervalo entre rescans

// --- Scanner BLE --------------------------------------------------------
#define BLE_SCAN_DURATION_S   5       // duracao de cada scan (scan->start)
#define BLE_SCAN_PAUSE_MS     3000    // pausa mostrando resultados antes do proximo scan

// --- RF Tamagotchi --------------------------------------------------------
#define TAMA_XP_BASE           60     // XP p/ sair do nivel 0 (era 40 -- evolucao mais lenta)
#define TAMA_XP_SLOPE          45     // XP extra por nivel acima do anterior (era 30)
#define TAMA_FOOD_CAP          45     // "calorias" maximas por ciclo de alimentacao (era 60)
#define TAMA_SLEEP_MS          60000  // sem NENHUMA comida (RF) por esse tempo -> bicho dorme
#define TAMA_SICK_MS           45000  // barriga zerada por esse tempo seguido -> bicho fica doente
#define TAMA_BLE_FEED_ENABLED  1      // BLE tambem alimenta (1 scan curto ~1s a cada ~12s).
                                       // Compartilha o radio 2.4GHz com o WiFi que ja roda nesse
                                       // app -- se notar travamento/reinicio no Tamagotchi, mude
                                       // para 0 e reporte, pode ser coexistencia WiFi+BLE instavel.

// --- Scanner 2.4GHz (NRF24) -------------------------------------------------
#define NRF_NUM_CHANNELS     80      // canais 0..79 -> ~2400..2479 MHz
#define NRF_DWELL_US         200     // tempo de escuta por canal

// --- Encoder ----------------------------------------------------------------
#define ENC_STEPS_PER_DETENT 2       // KY-040: normalmente 2 contagens por "clique"

// --- UI ---------------------------------------------------------------------
#define UI_ROTATION          1       // 0..3 ; 1 = paisagem (USB a direita)
#define COLOR_BG             0x0000  // preto
#define COLOR_FG             0xFFFF  // branco
#define COLOR_ACCENT         0x07FF  // ciano
#define COLOR_SEL            0x001F  // azul (item selecionado)
#define COLOR_WARN           0xFD20  // laranja
#define COLOR_OK             0x07E0  // verde

// --- Aviso de uso responsavel (mostrado no boot e em "About") ---------------
#define RESPONSIBLE_USE \
  "Use SOMENTE em redes e\n" \
  "equipamentos que voce possui\n" \
  "ou tem permissao para testar."
