#include "achievements.h"
#include "config.h"
#include <Preferences.h>

Achievements ach;
static Preferences prefs;

// indices simbolicos (so pra deixar report*()/unlock() legivel)
enum {
  A_FIRST_WIFI, A_WIFI_10, A_WIFI_100, A_WIFI_1000,
  A_FIRST_SWEEP, A_FIRST_CAPTURE, A_FIRST_BLE,
  A_UP_1H, A_UP_10H, A_UP_100H,
  A_PET_FERA
};

static const char *NAMES[ACH_COUNT] = {
  "Primeira rede", "10 redes", "100 redes", "1000 redes",
  "Primeiro sweep", "Primeira captura", "Primeiro BLE",
  "1h ligado", "10h ligado", "100h ligado",
  "Bichinho Fera"
};
static const char *DESCS[ACH_COUNT] = {
  "Encontre 1 rede WiFi no scanner",
  "Some 10 redes vistas (WiFi Scanner)",
  "Some 100 redes vistas (WiFi Scanner)",
  "Some 1000 redes vistas (WiFi Scanner)",
  "Use o Sub-GHz Sweep",
  "Grave um sinal no Sub-GHz Capturar/Repetir",
  "Rode o Scanner BLE",
  "1 hora de uso total do aparelho",
  "10 horas de uso total do aparelho",
  "100 horas de uso total do aparelho",
  "Evolua o RF Tamagotchi ate a forma Fera"
};

// =================================================================== NVS ====
void Achievements::begin() {
  static bool opened = false;
  if (!opened) { prefs.begin("ach", false); opened = true; }

  for (int i = 0; i < ACH_COUNT; i++) {
    _list[i].name = NAMES[i];
    _list[i].desc = DESCS[i];
    char key[8]; snprintf(key, sizeof(key), "u%d", i);
    _list[i].unlocked = prefs.getBool(key, false);
  }
  _wifiNetsTotal  = prefs.getUInt("wifiTot", 0);
  _uptimeSecTotal = prefs.getUInt("upTot", 0);
  _sessionStartMs = millis();
  _lastSaveMs     = millis();
}

void Achievements::save() {
  prefs.putUInt("wifiTot", _wifiNetsTotal);
  prefs.putUInt("upTot", _uptimeSecTotal);
}

void Achievements::unlock(int idx) {
  if (_list[idx].unlocked) return;
  _list[idx].unlocked = true;
  char key[8]; snprintf(key, sizeof(key), "u%d", idx);
  prefs.putBool(key, true);
  _lastUnlocked = _list[idx].name;
  _lastUnlockMs = millis();
  _pendingXp += 20;    // bonus de XP pro Tamagotchi, aplicado na proxima vez que ele abrir
}

int Achievements::drainXpBonus() {
  int x = _pendingXp;
  _pendingXp = 0;
  return x;
}

// =================================================================== eventos ==
void Achievements::reportWifiScan(int netsFound) {
  if (netsFound > 0) unlock(A_FIRST_WIFI);
  if (netsFound < 0) netsFound = 0;
  _wifiNetsTotal += (uint32_t)netsFound;
  save();
  if (_wifiNetsTotal >= 10)   unlock(A_WIFI_10);
  if (_wifiNetsTotal >= 100)  unlock(A_WIFI_100);
  if (_wifiNetsTotal >= 1000) unlock(A_WIFI_1000);
}

void Achievements::reportSweepUsed()         { unlock(A_FIRST_SWEEP); }
void Achievements::reportSubGhzCaptureUsed() { unlock(A_FIRST_CAPTURE); }
void Achievements::reportBleScanUsed()       { unlock(A_FIRST_BLE); }

void Achievements::reportTamagotchiLevel(int level) {
  if (level >= 6) unlock(A_PET_FERA);   // nivel 6..9 = estagio "Fera" (ver tamagotchi.cpp)
}

void Achievements::tick() {
  uint32_t now = millis();
  if (now - _lastSaveMs < 30000) return;      // banca o uptime a cada 30s (poupa a flash)
  _uptimeSecTotal += (now - _sessionStartMs) / 1000;
  _sessionStartMs = now;
  _lastSaveMs = now;
  save();

  uint32_t h = _uptimeSecTotal / 3600;
  if (h >= 1)   unlock(A_UP_1H);
  if (h >= 10)  unlock(A_UP_10H);
  if (h >= 100) unlock(A_UP_100H);
}

// =================================================================== desenho ==
void Achievements::selMove(int d) {
  _sel = ((_sel + d) % ACH_COUNT + ACH_COUNT) % ACH_COUNT;
}

String Achievements::progressText(int idx) const {
  if (_list[idx].unlocked) return "Concluida!";
  switch (idx) {
    case A_WIFI_10:   return String(_wifiNetsTotal) + "/10 redes";
    case A_WIFI_100:  return String(_wifiNetsTotal) + "/100 redes";
    case A_WIFI_1000: return String(_wifiNetsTotal) + "/1000 redes";
    case A_UP_1H:     return String(_uptimeSecTotal / 60) + "/60 min";
    case A_UP_10H:    return String(_uptimeSecTotal / 3600) + "/10 h";
    case A_UP_100H:   return String(_uptimeSecTotal / 3600) + "/100 h";
    default:          return "Pendente";
  }
}

void Achievements::draw(TFT_eSPI &tft, bool full) {
  const int ROW_H = 18, LIST_Y = 26, VISIBLE = 9;

  if (full) {
    tft.fillScreen(COLOR_BG);
    tft.fillRect(0, 0, tft.width(), 22, COLOR_SEL);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_FG, COLOR_SEL);
    tft.drawString("Achievements", 6, 4, 2);
  }

  int unlockedCount = 0;
  for (int i = 0; i < ACH_COUNT; i++) if (_list[i].unlocked) unlockedCount++;

  tft.fillRect(0, LIST_Y, tft.width(), ROW_H * VISIBLE, COLOR_BG);

  int start = _sel - VISIBLE / 2;
  if (start < 0) start = 0;
  if (start > ACH_COUNT - VISIBLE) start = max(0, ACH_COUNT - VISIBLE);

  tft.setTextDatum(TL_DATUM);
  for (int row = 0; row < VISIBLE && (start + row) < ACH_COUNT; row++) {
    int i = start + row;
    int y = LIST_Y + row * ROW_H;
    bool sel = (i == _sel);
    if (sel) tft.fillRect(2, y, tft.width() - 4, ROW_H - 1, COLOR_ACCENT);

    uint16_t fg = sel ? COLOR_BG : (_list[i].unlocked ? COLOR_OK : COLOR_FG);
    tft.setTextColor(fg, sel ? COLOR_ACCENT : COLOR_BG);
    char line[40];
    snprintf(line, sizeof(line), "%s %s", _list[i].unlocked ? "[X]" : "[ ]", _list[i].name);
    tft.drawString(line, 6, y + 2, 2);
  }

  // ---- rodape: detalhe da selecionada + contador geral ----
  const int FY = LIST_Y + ROW_H * VISIBLE + 4;
  tft.fillRect(0, FY, tft.width(), tft.height() - FY, COLOR_BG);
  tft.setTextColor(COLOR_ACCENT, COLOR_BG);
  tft.drawString(_list[_sel].desc, 6, FY, 1);
  tft.setTextColor(COLOR_FG, COLOR_BG);
  tft.drawString(progressText(_sel), 6, FY + 12, 1);

  char totals[24];
  snprintf(totals, sizeof(totals), "%d/%d desbloqueadas", unlockedCount, ACH_COUNT);
  tft.setTextColor(COLOR_OK, COLOR_BG);
  tft.drawString(totals, tft.width() - 130, FY, 1);

  tft.setTextColor(COLOR_FG, COLOR_BG);
  tft.drawString("girar=navega   [BACK] sair", 6, FY + 24, 1);

  // ---- toast de conquista recem-desbloqueada ----
  if (millis() - _lastUnlockMs < 2500 && _lastUnlocked[0]) {
    tft.fillRect(20, 90, tft.width() - 40, 40, COLOR_OK);
    tft.setTextColor(COLOR_BG, COLOR_OK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(String("Desbloqueado: ") + _lastUnlocked, tft.width() / 2, 110, 2);
    tft.setTextDatum(TL_DATUM);
  }
}
