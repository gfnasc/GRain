#include "display.h"

#include <Arduino.h>
#include <U8g2lib.h>

#include "config.h"

static const char *TAG = "display";

// Buffer completo (F): desenha tudo em RAM e envia de uma vez, sem piscar.
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C s_oled(U8G2_R0, U8X8_PIN_NONE);

enum Page { PAGE_TEMP, PAGE_PRESSURE, PAGE_DETAILS, PAGE_COUNT };

static bool s_ok = false;
static Reading s_last;
static bool s_hasReading = false;
static int s_page = 0;
static uint32_t s_pageSince = 0;

// Formata com uma casa decimal e vírgula (padrão brasileiro).
static void fmt1(char *out, size_t n, float v) {
  snprintf(out, n, "%.1f", v);
  for (char *p = out; *p; p++)
    if (*p == '.') *p = ',';
}

static void drawHeader(const char *title) {
  s_oled.setFont(u8g2_font_6x10_tf);
  s_oled.drawUTF8(0, 8, title);
  // Indicador de página: bolinhas no canto direito.
  for (int i = 0; i < PAGE_COUNT; i++) {
    int x = 128 - (PAGE_COUNT - i) * 7 + 2;
    if (i == s_page) s_oled.drawDisc(x, 4, 2);
    else s_oled.drawCircle(x, 4, 2);
  }
  s_oled.drawHLine(0, 11, 128);
}

// Número grande centralizado com unidade pequena ao lado.
static void drawBig(const char *value, const char *unit, int baseline) {
  s_oled.setFont(u8g2_font_logisoso28_tf);
  int wv = s_oled.getUTF8Width(value);
  s_oled.setFont(u8g2_font_7x13_tf);
  int wu = s_oled.getUTF8Width(unit);
  int x = (128 - (wv + 3 + wu)) / 2;
  s_oled.setFont(u8g2_font_logisoso28_tf);
  s_oled.drawUTF8(x, baseline, value);
  s_oled.setFont(u8g2_font_7x13_tf);
  s_oled.drawUTF8(x + wv + 3, baseline - 17, unit);
}

static void drawMissing(const char *what) {
  s_oled.setFont(u8g2_font_7x13_tf);
  int w = s_oled.getUTF8Width(what);
  s_oled.drawUTF8((128 - w) / 2, 36, what);
  s_oled.setFont(u8g2_font_6x10_tf);
  const char *hint = "sem leitura";
  w = s_oled.getUTF8Width(hint);
  s_oled.drawUTF8((128 - w) / 2, 52, hint);
}

static void drawTemp() {
  drawHeader("Temperatura");
  if (!s_last.ahtOk) return drawMissing("AHT20");
  char v[12], h[12], hi[12], line[32];
  fmt1(v, sizeof v, s_last.tempC);
  drawBig(v, "°C", 46);
  snprintf(h, sizeof h, "%.0f", s_last.humPct);
  fmt1(hi, sizeof hi, s_last.heatIndexC);
  snprintf(line, sizeof line, "Umid %s%%  Sens %s°", h, hi);
  s_oled.setFont(u8g2_font_6x10_tf);
  int w = s_oled.getUTF8Width(line);
  s_oled.drawUTF8((128 - w) / 2, 63, line);
}

static void drawPressure() {
  drawHeader("Pressão");
  if (!s_last.bmpOk) return drawMissing("BMP280");
  char v[12], local[12], line[32];
  snprintf(v, sizeof v, "%.0f", s_last.seaLevelHpa);
  drawBig(v, "hPa", 46);
  fmt1(local, sizeof local, s_last.stationHpa);
  snprintf(line, sizeof line, "Local %s hPa", local);
  s_oled.setFont(u8g2_font_6x10_tf);
  int w = s_oled.getUTF8Width(line);
  s_oled.drawUTF8((128 - w) / 2, 63, line);
}

static void drawRow(int y, const char *label, const char *value) {
  s_oled.drawUTF8(0, y, label);
  int w = s_oled.getUTF8Width(value);
  s_oled.drawUTF8(128 - w, y, value);
}

static void drawDetails() {
  drawHeader("Detalhes");
  s_oled.setFont(u8g2_font_6x10_tf);
  char buf[20], num[12];

  if (s_last.ahtOk) {
    fmt1(num, sizeof num, s_last.dewPointC);
    snprintf(buf, sizeof buf, "%s °C", num);
  } else {
    snprintf(buf, sizeof buf, "--");
  }
  drawRow(24, "Ponto de orvalho", buf);

  if (s_last.bmpOk) {
    fmt1(num, sizeof num, s_last.tempBmpC);
    snprintf(buf, sizeof buf, "%s °C", num);
  } else {
    snprintf(buf, sizeof buf, "--");
  }
  drawRow(37, "Temp. BMP280", buf);

  snprintf(buf, sizeof buf, "%s %s", s_last.ahtOk ? "ok" : "ERRO", s_last.bmpOk ? "ok" : "ERRO");
  drawRow(50, "AHT20 BMP280", buf);

  uint32_t min = millis() / 60000;
  snprintf(buf, sizeof buf, "%luh%02lum", (unsigned long)(min / 60), (unsigned long)(min % 60));
  drawRow(63, "Ligado há", buf);
}

static void render() {
  if (!s_ok) return;
  s_oled.clearBuffer();
  if (!s_hasReading) {
    s_oled.setFont(u8g2_font_7x13_tf);
    const char *msg = "Iniciando...";
    s_oled.drawUTF8((128 - s_oled.getUTF8Width(msg)) / 2, 38, msg);
  } else {
    switch (s_page) {
      case PAGE_TEMP: drawTemp(); break;
      case PAGE_PRESSURE: drawPressure(); break;
      case PAGE_DETAILS: drawDetails(); break;
    }
  }
  s_oled.sendBuffer();
}

bool displayBegin() {
  // Mantém o barramento na mesma velocidade dos sensores.
  s_oled.setBusClock(I2C_FREQ_HZ);
  s_oled.setI2CAddress(ADDR_OLED << 1);
  s_ok = s_oled.begin();
  if (!s_ok) {
    log_e("[%s] OLED não respondeu em 0x%02X", TAG, ADDR_OLED);
    return false;
  }
  s_oled.setContrast(DISPLAY_CONTRAST);
  s_oled.enableUTF8Print();
  s_pageSince = millis();
  render();
  log_i("[%s] OLED ok em 0x%02X", TAG, ADDR_OLED);
  return true;
}

void displayUpdate(const Reading &r) {
  s_last = r;
  s_hasReading = true;
  render();
}

void displayLoop() {
  uint32_t now = millis();
  if ((uint32_t)(now - s_pageSince) >= DISPLAY_PAGE_MS) {
    s_pageSince = now;
    s_page = (s_page + 1) % PAGE_COUNT;
    render();
  }
}
