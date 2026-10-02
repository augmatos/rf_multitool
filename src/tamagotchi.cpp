#include "tamagotchi.h"
#include "config.h"
#include <Preferences.h>
#include <math.h>

// ---- cores do corpo por estágio (RGB565; ajuste à vontade) ----
static const uint16_t BODY_COLOR[7] = {
  0xFF99,  // 0 Ovo    - creme
  0x8F2F,  // 1 Blobi  - verde claro
  0x5B7C,  // 2 Pixel  - ciano
  0xF4A5,  // 3 Fera   - laranja
  0xBD1E,  // 4 Anciao - lavanda
  0xFEA0,  // 5 Lenda  - dourado
  0x781F   // 6 Mestre - roxo profundo
};
static const char *STAGE_NAME[7] = { "Ovo", "Blobi", "Pixel", "Fera", "Anciao", "Lenda", "Mestre" };

enum Mood { M_EAT, M_SMILE, M_NEUTRAL, M_SAD, M_SICK, M_SLEEP };

static Preferences prefs;

// =================================================================== regras ==
int Tamagotchi::xpForNext() const { return TAMA_XP_BASE + _level * TAMA_XP_SLOPE; }

int Tamagotchi::stage() const {
  if (_level <= 0)   return 0;   // Ovo
  if (_level <= 3)   return 1;   // Blobi
  if (_level <= 7)   return 2;   // Pixel
  if (_level <= 12)  return 3;   // Fera
  if (_level <= 18)  return 4;   // Anciao
  if (_level <= 25)  return 5;   // Lenda
  return 6;                      // Mestre
}
const char *Tamagotchi::stageName() const { return STAGE_NAME[stage()]; }

void Tamagotchi::begin() {
  static bool opened = false;                 // abre o NVS uma única vez por boot
  if (!opened) { prefs.begin("tama", false); opened = true; }
  _xp         = prefs.getUInt("xp", 0);
  _level      = prefs.getInt("lvl", 0);
  _fullness   = prefs.getInt("full", 60);
  _happiness  = prefs.getInt("happy", 60);
  _ageSec     = prefs.getUInt("age", 0);
  _maxLevel   = prefs.getInt("maxlvl", _level);
  _evolutions = prefs.getInt("evo", 0);
  _prevStage  = stage();
  _lastUpdateMs = millis();
  _lastFeedMs   = millis();
  _fullZeroSinceMs = 0;
}

void Tamagotchi::save() {
  prefs.putUInt("xp", _xp);
  prefs.putInt("lvl", _level);
  prefs.putInt("full", _fullness);
  prefs.putInt("happy", _happiness);
  prefs.putUInt("age", _ageSec);
  prefs.putInt("maxlvl", _maxLevel);
  prefs.putInt("evo", _evolutions);
}

// sobe de nivel enquanto houver XP suficiente; atualiza recorde de nivel e
// contador de evolucoes quando o estagio muda
void Tamagotchi::feed(int wifiNets, int nrfActiveCh, int subghzHit, int bleDevices) {
  _lastNets = wifiNets;
  _lastHits = nrfActiveCh;
  _lastSub  = subghzHit;
  _lastBle  = bleDevices;
  int food = wifiNets * 3 + nrfActiveCh * 2 + subghzHit * 15 + bleDevices * 4;  // "calorias"
  if (food <= 0) {                                 // ambiente vazio -> fome
    _react = "fome...";
    _reactMs = millis();
    return;
  }
  food = constrain(food, 0, TAMA_FOOD_CAP);
  _xp += food;
  _fullness = min(100, _fullness + min(food, 25));
  _eatMs  = millis();
  _react  = "nhac!";
  _reactMs = millis();
  _lastFeedMs = millis();      // reseta o contador de "tempo sem comida" (dormindo)

  while (_xp >= (uint32_t)xpForNext()) {
    _xp -= xpForNext();
    _level++;
    _happiness = min(100, _happiness + 10);
  }
  if (_level > _maxLevel) _maxLevel = _level;
  if (stage() != _prevStage) {
    _prevStage = stage();
    _evolutions++;
    _react = "EVOLUIU!";
    _reactMs = millis();
    save();
  }
}

void Tamagotchi::addXp(int amount) {
  if (amount <= 0) return;
  _xp += (uint32_t)amount;
  _react = "bonus!";
  _reactMs = millis();

  while (_xp >= (uint32_t)xpForNext()) {
    _xp -= xpForNext();
    _level++;
    _happiness = min(100, _happiness + 10);
  }
  if (_level > _maxLevel) _maxLevel = _level;
  if (stage() != _prevStage) {
    _prevStage = stage();
    _evolutions++;
    _react = "EVOLUIU!";
    _reactMs = millis();
  }
  save();
}

void Tamagotchi::petLove() {
  _happiness = min(100, _happiness + 8);
  _xp += 1;
  _react = "<3";
  _reactMs = millis();
}

void Tamagotchi::update() {
  uint32_t now = millis();
  uint32_t dt = now - _lastUpdateMs;
  _lastUpdateMs = now;

  // idade
  _ageAccumMs += dt;
  while (_ageAccumMs >= 1000) { _ageAccumMs -= 1000; _ageSec++; }

  // barriga decai ~1 ponto a cada 4 s
  _fullAccumMs += dt;
  while (_fullAccumMs >= 4000) { _fullAccumMs -= 4000; if (_fullness > 0) _fullness--; }

  // humor tende a um alvo definido pela barriga, ~1 ponto/s
  _happyAccumMs += dt;
  while (_happyAccumMs >= 1000) {
    _happyAccumMs -= 1000;
    int target = (_fullness > 60) ? 85 : (_fullness > 30) ? 50 : 18;
    if (_happiness < target) _happiness++;
    else if (_happiness > target) _happiness--;
  }

  // marca desde quando a barriga esta em 0 (base do estado "doente" em drawPet)
  if (_fullness == 0) { if (_fullZeroSinceMs == 0) _fullZeroSinceMs = now; }
  else _fullZeroSinceMs = 0;
}

// =================================================================== desenho ==
static void drawBar(TFT_eSPI &tft, int x, int y, int w, int h, int pct, uint16_t color) {
  pct = constrain(pct, 0, 100);
  tft.drawRect(x, y, w, h, COLOR_FG);
  int fw = (w - 2) * pct / 100;
  tft.fillRect(x + 1, y + 1, fw, h - 2, color);
  tft.fillRect(x + 1 + fw, y + 1, (w - 2) - fw, h - 2, COLOR_BG);
}

void Tamagotchi::drawPet(TFT_eSPI &tft, int cx, int cy) {
  int st = stage();
  uint16_t body = BODY_COLOR[st];
  uint32_t now = millis();

  bool sleeping = (now - _lastFeedMs) > TAMA_SLEEP_MS;
  bool sick     = (_fullZeroSinceMs != 0) && (now - _fullZeroSinceMs) > TAMA_SICK_MS;

  // humor -- EAT sempre tem prioridade (feedback imediato de ter comido, o
  // que ja limpa sleeping/sick por baixo dos panos via _lastFeedMs/fullness)
  Mood mood;
  if (now - _eatMs < 700)           mood = M_EAT;
  else if (sick)                    mood = M_SICK;
  else if (sleeping)                mood = M_SLEEP;
  else if (_happiness >= 60)        mood = M_SMILE;
  else if (_happiness >= 30)        mood = M_NEUTRAL;
  else                               mood = M_SAD;

  bool blink = (now % 4000) < 150;               // pisca ~150ms a cada 4s
  int  bob   = sleeping ? 0 : (int)(2.5f * sinf(now / 280.0f));  // sem balanco dormindo
  cy += bob;

  // ---------- corpo por estágio ----------
  switch (st) {
    case 0: // Ovo
      tft.fillEllipse(cx, cy + 4, 34, 42, body);
      tft.drawEllipse(cx, cy + 4, 34, 42, COLOR_FG);
      tft.fillCircle(cx - 12, cy - 2, 4, 0xFFFF & ~body);
      tft.fillCircle(cx + 10, cy + 14, 3, 0xFFFF & ~body);
      break;
    case 1: // Blobi
      tft.fillSmoothCircle(cx, cy, 40, body, COLOR_BG);
      tft.fillCircle(cx - 18, cy + 36, 8, body);   // pezinhos
      tft.fillCircle(cx + 18, cy + 36, 8, body);
      break;
    case 2: // Pixel
      tft.fillSmoothCircle(cx, cy, 42, body, COLOR_BG);
      tft.fillTriangle(cx - 38, cy - 28, cx - 14, cy - 44, cx - 8, cy - 22, body); // orelhas
      tft.fillTriangle(cx + 38, cy - 28, cx + 14, cy - 44, cx + 8, cy - 22, body);
      tft.fillCircle(cx - 20, cy + 38, 8, body);
      tft.fillCircle(cx + 20, cy + 38, 8, body);
      break;
    case 3: // Fera
      tft.fillSmoothRoundRect(cx - 46, cy - 40, 92, 84, 22, body, COLOR_BG);
      tft.fillTriangle(cx - 40, cy - 36, cx - 22, cy - 64, cx - 10, cy - 38, body); // chifres
      tft.fillTriangle(cx + 40, cy - 36, cx + 22, cy - 64, cx + 10, cy - 38, body);
      tft.fillRect(cx - 30, cy + 40, 14, 14, body);
      tft.fillRect(cx + 16, cy + 40, 14, 14, body);
      break;
    case 4: // Anciao
      tft.fillSmoothCircle(cx, cy, 44, body, COLOR_BG);
      tft.fillTriangle(cx - 22, cy + 6, cx + 22, cy + 6, cx, cy + 56, COLOR_FG); // barba
      tft.drawCircle(cx - 14, cy - 6, 10, COLOR_FG);   // óculos
      tft.drawCircle(cx + 14, cy - 6, 10, COLOR_FG);
      tft.drawLine(cx - 4, cy - 6, cx + 4, cy - 6, COLOR_FG);
      break;
    case 5: // Lenda -- corpo com "aura" de aneis
      tft.fillSmoothCircle(cx, cy, 44, body, COLOR_BG);
      tft.drawCircle(cx, cy, 50, COLOR_ACCENT);
      tft.drawCircle(cx, cy, 55, COLOR_ACCENT);
      tft.fillTriangle(cx - 52, cy, cx - 30, cy - 30, cx - 18, cy + 8, body);  // asas
      tft.fillTriangle(cx + 52, cy, cx + 30, cy - 30, cx + 18, cy + 8, body);
      break;
    default: // 6 Mestre -- forma final, "coroa"/antena no topo
      tft.fillSmoothRoundRect(cx - 48, cy - 42, 96, 88, 26, body, COLOR_BG);
      tft.fillTriangle(cx - 10, cy - 42, cx, cy - 66, cx + 10, cy - 42, COLOR_WARN); // antena/coroa
      tft.fillCircle(cx, cy - 66, 4, COLOR_WARN);
      tft.fillCircle(cx - 32, cy + 42, 10, body);
      tft.fillCircle(cx + 32, cy + 42, 10, body);
      break;
  }

  // ---------- olhos ----------
  int eyeY = cy - 6, dx = 14;
  if (st == 0) eyeY = cy - 2;
  if (mood == M_SLEEP) {                                       // fechados o tempo todo
    tft.drawFastHLine(cx - dx - 5, eyeY, 10, COLOR_FG);
    tft.drawFastHLine(cx + dx - 5, eyeY, 10, COLOR_FG);
  } else if (mood == M_SICK) {                                 // olhos em X
    tft.drawLine(cx - dx - 5, eyeY - 4, cx - dx + 5, eyeY + 4, COLOR_WARN);
    tft.drawLine(cx - dx - 5, eyeY + 4, cx - dx + 5, eyeY - 4, COLOR_WARN);
    tft.drawLine(cx + dx - 5, eyeY - 4, cx + dx + 5, eyeY + 4, COLOR_WARN);
    tft.drawLine(cx + dx - 5, eyeY + 4, cx + dx + 5, eyeY - 4, COLOR_WARN);
  } else if (blink) {
    tft.drawFastHLine(cx - dx - 5, eyeY, 10, COLOR_FG);
    tft.drawFastHLine(cx + dx - 5, eyeY, 10, COLOR_FG);
  } else if (mood == M_SMILE && _happiness > 80) {           // olhos felizes ^ ^
    tft.drawLine(cx - dx - 5, eyeY + 3, cx - dx, eyeY - 3, COLOR_FG);
    tft.drawLine(cx - dx, eyeY - 3, cx - dx + 5, eyeY + 3, COLOR_FG);
    tft.drawLine(cx + dx - 5, eyeY + 3, cx + dx, eyeY - 3, COLOR_FG);
    tft.drawLine(cx + dx, eyeY - 3, cx + dx + 5, eyeY + 3, COLOR_FG);
  } else {
    int droop = (mood == M_SAD) ? 3 : 0;
    tft.fillCircle(cx - dx, eyeY + droop, 6, COLOR_FG);
    tft.fillCircle(cx + dx, eyeY + droop, 6, COLOR_FG);
    tft.fillCircle(cx - dx + 1, eyeY + droop + 1, 2, body); // brilho
    tft.fillCircle(cx + dx + 1, eyeY + droop + 1, 2, body);
  }

  // ---------- boca ----------
  int my = cy + 16;
  switch (mood) {
    case M_EAT:                                   // boca aberta
      tft.fillCircle(cx, my, 6, COLOR_FG);
      break;
    case M_SMILE:                                 // sorriso (arco inferior)
      tft.drawCircleHelper(cx, my - 4, 11, 0xC, COLOR_FG);
      tft.drawCircleHelper(cx, my - 4, 10, 0xC, COLOR_FG);
      break;
    case M_NEUTRAL:
      tft.drawFastHLine(cx - 9, my, 18, COLOR_FG);
      break;
    case M_SAD:                                   // boca pra baixo (arco superior)
      tft.drawCircleHelper(cx, my + 6, 11, 0x3, COLOR_FG);
      tft.drawCircleHelper(cx, my + 6, 10, 0x3, COLOR_FG);
      break;
    case M_SICK:                                  // boca em ziguezague (enjoado)
      tft.drawLine(cx - 9, my,     cx - 3, my - 4, COLOR_WARN);
      tft.drawLine(cx - 3, my - 4, cx + 3, my,     COLOR_WARN);
      tft.drawLine(cx + 3, my,     cx + 9, my - 4, COLOR_WARN);
      break;
    case M_SLEEP:                                 // "o" pequeno (roncando) + Zzz
      tft.fillCircle(cx, my, 3, COLOR_FG);
      tft.setTextColor(COLOR_ACCENT, COLOR_BG);
      tft.drawString("z Z z", cx + 22, cy - 58, 2);
      break;
  }
}

void Tamagotchi::draw(TFT_eSPI &tft, bool full, bool showStats) {
  if (showStats) { drawStats(tft, full); return; }

  const int PX = 6, PY = 24, PW = 200, PH = 184;   // caixa do bicho
  const int RX = 214;                              // painel direito

  if (full) {
    tft.fillScreen(COLOR_BG);
    tft.fillRect(0, 0, tft.width(), 20, COLOR_SEL);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_FG, COLOR_SEL);
    tft.drawString("RF Tamagotchi", 6, 3, 2);
    tft.setTextColor(COLOR_FG, COLOR_BG);
    tft.drawString("girar/clique=carinho AUX=stats BACK=sai", 4, 224, 1);
  }

  // ---- bicho ----
  tft.fillRect(PX, PY, PW, PH, COLOR_BG);
  drawPet(tft, PX + PW / 2, PY + PH / 2 - 6);

  // ---- "fala" flutuante ----
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COLOR_ACCENT, COLOR_BG);
  bool talking = (millis() - _reactMs < 900) && _react[0];
  char talk[16];
  snprintf(talk, sizeof(talk), "%-10s", talking ? _react : "");
  tft.drawString(talk, PX + 16, PY + 6, 4);

  // ---- painel de status ----
  char buf[24];
  tft.setTextColor(COLOR_FG, COLOR_BG);
  snprintf(buf, sizeof(buf), "Lv %-3d", _level);
  tft.drawString(buf, RX, 28, 4);
  snprintf(buf, sizeof(buf), "%-8s", stageName());
  tft.drawString(buf, RX, 56, 2);

  tft.drawString("XP", RX, 76, 1);
  drawBar(tft, RX, 88, 98, 10, (int)(_xp * 100 / (uint32_t)xpForNext()), COLOR_ACCENT);

  tft.drawString("Cheio", RX, 104, 1);
  drawBar(tft, RX, 116, 98, 10, _fullness, COLOR_OK);

  tft.drawString("Feliz", RX, 132, 1);
  drawBar(tft, RX, 144, 98, 10, _happiness,
          _happiness >= 30 ? COLOR_OK : COLOR_WARN);

  snprintf(buf, sizeof(buf), "Idade: %lum%02lus", (unsigned long)(_ageSec / 60),
           (unsigned long)(_ageSec % 60));
  tft.drawString(buf, RX, 164, 1);
  snprintf(buf, sizeof(buf), "WiFi:%-2d  2.4G:%-2d", _lastNets, _lastHits);
  tft.drawString(buf, RX, 180, 1);
  snprintf(buf, sizeof(buf), "Sub:%-2d  BLE:%-2d", _lastSub, _lastBle);
  tft.drawString(buf, RX, 196, 1);
}

// =================================================================== stats ==
void Tamagotchi::drawStats(TFT_eSPI &tft, bool full) {
  if (full) {
    tft.fillScreen(COLOR_BG);
    tft.fillRect(0, 0, tft.width(), 20, COLOR_SEL);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_FG, COLOR_SEL);
    tft.drawString("Estatisticas", 6, 3, 2);
    tft.setTextColor(COLOR_FG, COLOR_BG);
    tft.drawString("AUX = voltar pro bicho", 4, 224, 1);
  }

  tft.setTextDatum(TL_DATUM);
  char b[40];
  int y = 32;

  tft.setTextColor(COLOR_ACCENT, COLOR_BG);
  snprintf(b, sizeof(b), "Nivel atual:   %-4d", _level);
  tft.drawString(b, 12, y, 2); y += 20;
  snprintf(b, sizeof(b), "Nivel maximo:  %-4d", _maxLevel);
  tft.drawString(b, 12, y, 2); y += 20;

  tft.setTextColor(COLOR_FG, COLOR_BG);
  snprintf(b, sizeof(b), "Estagio atual: %-10s", stageName());
  tft.drawString(b, 12, y, 2); y += 20;
  snprintf(b, sizeof(b), "Evolucoes:     %-4d", _evolutions);
  tft.drawString(b, 12, y, 2); y += 20;
  snprintf(b, sizeof(b), "Idade total:   %lum%02lus     ",
           (unsigned long)(_ageSec / 60), (unsigned long)(_ageSec % 60));
  tft.drawString(b, 12, y, 2); y += 28;

  tft.setTextColor(COLOR_ACCENT, COLOR_BG);
  tft.drawString("Ultima leitura de RF:", 12, y, 2); y += 20;
  tft.setTextColor(COLOR_FG, COLOR_BG);
  snprintf(b, sizeof(b), "WiFi:%-3d   2.4GHz:%-3d", _lastNets, _lastHits);
  tft.drawString(b, 12, y, 2); y += 18;
  snprintf(b, sizeof(b), "Sub-GHz:%-3d  BLE:%-3d", _lastSub, _lastBle);
  tft.drawString(b, 12, y, 2);
}
