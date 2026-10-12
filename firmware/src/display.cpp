#include "display.h"

#include <Arduino.h>
#include <U8g2lib.h>

#include "config.h"
#include "netmgr.h"
#include "settings.h"
#include "uplink.h"

static const char *TAG = "display";

// Buffer completo (F): desenha tudo em RAM e envia de uma vez, sem piscar.
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C s_oled(U8G2_R0, U8X8_PIN_NONE);

static bool s_ok = false;
static Reading s_last;
static bool s_hasReading = false;
static int s_page = 0;
static uint32_t s_pageSince = 0;
static int s_night = -1;  // -1 = não aplicado, 0 = dia, 1 = noite

static int pageCount() { return settings().pageCount ? settings().pageCount : 1; }

// Formata com uma casa decimal e vírgula (padrão brasileiro).
static void fmt1(char *out, size_t n, float v) {
  snprintf(out, n, "%.1f", v);
  for (char *p = out; *p; p++)
    if (*p == '.') *p = ',';
}

static void drawHeader(const char *title) {
  s_oled.setFont(u8g2_font_6x10_tf);
  s_oled.drawUTF8(0, 8, title);
  int titleW = s_oled.getUTF8Width(title);
  // Indicador de página: bolinhas no canto direito (menores com muitas páginas).
  int n = pageCount();
  int step = n > 5 ? 5 : 7, r = n > 5 ? 1 : 2;
  if (n > 1) {
    for (int i = 0; i < n; i++) {
      int x = 128 - (n - i) * step + step / 2;
      if (i == s_page) s_oled.drawDisc(x, 4, r + (n > 5 ? 1 : 0));
      else s_oled.drawCircle(x, 4, r);
    }
  }
  // Relógio (depois da sincronização), antes das bolinhas, se couber.
  char hhmm[8];
  if (netLocalTime(hhmm, sizeof hhmm, "%H:%M")) {
    s_oled.setFont(u8g2_font_5x8_tf);
    int w = s_oled.getUTF8Width(hhmm);
    int x = 128 - (n > 1 ? n * step : 0) - 3 - w;
    if (x > titleW + 4) s_oled.drawUTF8(x, 8, hhmm);
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

static void drawHumidity() {
  drawHeader("Umidade");
  if (!s_last.ahtOk) return drawMissing("AHT20");
  char v[12], dew[12], line[32];
  snprintf(v, sizeof v, "%.0f", s_last.humPct);
  drawBig(v, "%", 46);
  fmt1(dew, sizeof dew, s_last.dewPointC);
  snprintf(line, sizeof line, "Ponto de orvalho %s°", dew);
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

// Relógio grande, data e leitura resumida.
static void drawClock() {
  char t[8], d[24], line[32];
  bool ok = netLocalTime(t, sizeof t, "%H:%M");
  s_oled.setFont(u8g2_font_logisoso28_tf);
  const char *tt = ok ? t : "--:--";
  s_oled.drawUTF8((128 - s_oled.getUTF8Width(tt)) / 2, 34, tt);
  s_oled.setFont(u8g2_font_6x10_tf);
  if (ok) {
    static const char *DIAS[] = {"dom", "seg", "ter", "qua", "qui", "sex", "sáb"};
    time_t now = time(nullptr);
    struct tm lt;
    localtime_r(&now, &lt);
    snprintf(d, sizeof d, "%s, %02d/%02d/%04d", DIAS[lt.tm_wday], lt.tm_mday, lt.tm_mon + 1, lt.tm_year + 1900);
  } else {
    snprintf(d, sizeof d, "aguardando a hora");
  }
  s_oled.drawUTF8((128 - s_oled.getUTF8Width(d)) / 2, 48, d);
  if (s_last.ahtOk) {
    char temp[12];
    fmt1(temp, sizeof temp, s_last.tempC);
    snprintf(line, sizeof line, "%s °C   %.0f %%", temp, s_last.humPct);
    s_oled.drawUTF8((128 - s_oled.getUTF8Width(line)) / 2, 63, line);
  }
}

// Mínima e máxima de hoje (desde a meia-noite).
static void drawToday() {
  drawHeader("Hoje");
  s_oled.setFont(u8g2_font_6x10_tf);
  const TodayStats &d = sensorsToday();
  if (!d.valid) {
    s_oled.drawUTF8(0, 37, "aguardando dados");
    return;
  }
  char buf[24], num[12], hm[8];
  struct tm lt;
  time_t e = d.tMinTs;
  localtime_r(&e, &lt);
  strftime(hm, sizeof hm, "%H:%M", &lt);
  fmt1(num, sizeof num, d.tMin);
  snprintf(buf, sizeof buf, "%s° às %s", num, hm);
  drawRow(24, "Mínima", buf);
  e = d.tMaxTs;
  localtime_r(&e, &lt);
  strftime(hm, sizeof hm, "%H:%M", &lt);
  fmt1(num, sizeof num, d.tMax);
  snprintf(buf, sizeof buf, "%s° às %s", num, hm);
  drawRow(37, "Máxima", buf);
  snprintf(buf, sizeof buf, "%.0f a %.0f %%", d.hMin, d.hMax);
  drawRow(50, "Umidade", buf);
  snprintf(buf, sizeof buf, "%.0f °C", d.tMax - d.tMin);
  drawRow(63, "Amplitude", buf);
}

// Página "Resumo": até 4 campos escolhidos no painel.
static void drawFields(const PageCfg &pg) {
  drawHeader("Resumo");
  s_oled.setFont(u8g2_font_6x10_tf);
  static const char *LABELS[FD_COUNT] = {"Temperatura", "Umidade", "Pressão", "Orvalho", "Sensação", "Temp. BMP280", "Pressão local"};
  for (int i = 0; i < pg.nfields && i < MAX_FIELDS; i++) {
    char buf[20], num[12];
    uint8_t f = pg.fields[i];
    bool aht = s_last.ahtOk, bmp = s_last.bmpOk;
    switch (f) {
      case FD_TEMP: if (aht) { fmt1(num, sizeof num, s_last.tempC); snprintf(buf, sizeof buf, "%s °C", num); } break;
      case FD_HUM: if (aht) snprintf(buf, sizeof buf, "%.0f %%", s_last.humPct); break;
      case FD_PRES: if (bmp) snprintf(buf, sizeof buf, "%.0f hPa", s_last.seaLevelHpa); break;
      case FD_DEW: if (aht) { fmt1(num, sizeof num, s_last.dewPointC); snprintf(buf, sizeof buf, "%s °C", num); } break;
      case FD_HEAT: if (aht) { fmt1(num, sizeof num, s_last.heatIndexC); snprintf(buf, sizeof buf, "%s °C", num); } break;
      case FD_TEMP_BMP: if (bmp) { fmt1(num, sizeof num, s_last.tempBmpC); snprintf(buf, sizeof buf, "%s °C", num); } break;
      case FD_PRES_STATION: if (bmp) { fmt1(num, sizeof num, s_last.stationHpa); snprintf(buf, sizeof buf, "%s hPa", num); } break;
    }
    bool has = (f == FD_PRES || f == FD_TEMP_BMP || f == FD_PRES_STATION) ? bmp : aht;
    drawRow(24 + i * 13, f < FD_COUNT ? LABELS[f] : "?", has ? buf : "--");
  }
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

static void drawNetwork() {
  drawHeader("Rede");
  s_oled.setFont(u8g2_font_6x10_tf);
  const NetStatus &n = netStatus();
  char buf[24];

  if (n.connected) {
    snprintf(buf, sizeof buf, "%.14s", n.ssid.c_str());
    drawRow(24, "Wi-Fi", buf);
    drawRow(37, "IP", n.ip.c_str());
  } else {
    drawRow(24, "Wi-Fi", "desconectado");
    drawRow(37, "IP", "--");
  }
  // Estado do envio para a nuvem.
  const UplinkStatus &u = uplinkStatus();
  if (!u.configured) {
    snprintf(buf, sizeof buf, "desligada");
  } else if (u.lastError.length()) {
    snprintf(buf, sizeof buf, "erro (fila %u)", (unsigned)u.queue);
  } else if (u.lastOkEpoch) {
    time_t e = u.lastOkEpoch;
    struct tm lt;
    localtime_r(&e, &lt);
    strftime(buf, sizeof buf, "ok %H:%M", &lt);
  } else {
    snprintf(buf, sizeof buf, "aguardando");
  }
  drawRow(50, "Nuvem", buf);
  if (!netLocalTime(buf, sizeof buf, "%d/%m %H:%M")) snprintf(buf, sizeof buf, "aguardando");
  drawRow(63, "Hora", buf);
}

// Tela fixa enquanto a rede de configuração do Wi-Fi estiver aberta.
static void drawPortal() {
  const NetStatus &n = netStatus();
  s_oled.setFont(u8g2_font_6x10_tf);
  s_oled.drawUTF8(0, 8, "Configurar Wi-Fi");
  s_oled.drawHLine(0, 11, 128);
  s_oled.drawUTF8(0, 24, "No celular, conecte em:");
  s_oled.setFont(u8g2_font_7x13B_tf);
  s_oled.drawUTF8(0, 37, n.apName.c_str());
  s_oled.setFont(u8g2_font_6x10_tf);
  char buf[32];
  snprintf(buf, sizeof buf, "Senha: %s", n.apPass.c_str());
  s_oled.drawUTF8(0, 50, buf);
  s_oled.drawUTF8(0, 63, "Depois: 192.168.4.1");
}

// --- Tela de abertura: "Iniciando..." com spinner ---
// O spinner ocupa só os tiles 6..9 x 1..4 (32x32 px), atualizados com
// updateDisplayArea para não reenviar a tela inteira a cada quadro.
static const int SPIN_CX = 64, SPIN_CY = 24, SPIN_R = 11, SPIN_DOTS = 8;
static uint32_t s_splashUntil = 0;
static int s_spinFrame = -1;

static bool splashActive() { return s_splashUntil != 0 && (int32_t)(millis() - s_splashUntil) < 0; }

static void drawSpinner(int frame) {
  s_oled.setDrawColor(0);
  s_oled.drawBox(48, 8, 32, 32);
  s_oled.setDrawColor(1);
  for (int i = 0; i < SPIN_DOTS; i++) {
    float a = i * 2.0f * PI / SPIN_DOTS - PI / 2;
    int x = SPIN_CX + (int)lroundf(SPIN_R * cosf(a));
    int y = SPIN_CY + (int)lroundf(SPIN_R * sinf(a));
    int age = (frame - i + SPIN_DOTS) % SPIN_DOTS;  // 0 = ponto da frente
    if (age == 0) s_oled.drawDisc(x, y, 3);
    else if (age == 1) s_oled.drawDisc(x, y, 2);
    else if (age == 2) s_oled.drawDisc(x, y, 1);
    else s_oled.drawPixel(x, y);
  }
}

static void drawSplashFull() {
  s_oled.clearBuffer();
  drawSpinner(s_spinFrame < 0 ? 0 : s_spinFrame);
  s_oled.setFont(u8g2_font_7x13_tf);
  const char *msg = "Iniciando...";
  s_oled.drawUTF8((128 - s_oled.getUTF8Width(msg)) / 2, 58, msg);
  s_oled.sendBuffer();
}

static void render() {
  if (!s_ok) return;
  if (splashActive()) return drawSplashFull();
  s_oled.clearBuffer();
  if (netStatus().portalActive) {
    drawPortal();
  } else if (!s_hasReading) {
    s_oled.setFont(u8g2_font_7x13_tf);
    const char *msg = "Iniciando...";
    s_oled.drawUTF8((128 - s_oled.getUTF8Width(msg)) / 2, 38, msg);
  } else {
    if (s_page >= pageCount()) s_page = 0;
    const PageCfg &pg = settings().pages[s_page];
    switch (settings().pageCount ? pg.type : PG_TEMP) {
      case PG_TEMP: drawTemp(); break;
      case PG_HUM: drawHumidity(); break;
      case PG_PRES: drawPressure(); break;
      case PG_CLOCK: drawClock(); break;
      case PG_TODAY: drawToday(); break;
      case PG_FIELDS: drawFields(pg); break;
      case PG_DETAILS: drawDetails(); break;
      case PG_NETWORK: drawNetwork(); break;
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
  s_oled.setContrast(settings().displayContrast);
  s_oled.enableUTF8Print();
  s_splashUntil = millis() + DISPLAY_SPLASH_MS;
  if (s_splashUntil == 0) s_splashUntil = 1;
  s_spinFrame = 0;
  render();
  log_i("[%s] OLED ok em 0x%02X", TAG, ADDR_OLED);
  return true;
}

void displayUpdate(const Reading &r) {
  s_last = r;
  s_hasReading = true;
  render();
}

// Modo noturno: apaga ou escurece o display no horário configurado.
static void applyNight() {
  static uint32_t lastCheck = 0;
  uint32_t ms = millis();
  if (s_night != -1 && (uint32_t)(ms - lastCheck) < 1000) return;
  lastCheck = ms;
  const Settings &c = settings();
  int night = 0;
  if (c.nightEnabled && netStatus().timeSynced) {
    time_t now = time(nullptr);
    struct tm lt;
    localtime_r(&now, &lt);
    int m = lt.tm_hour * 60 + lt.tm_min;
    night = c.nightFrom > c.nightTo ? (m >= c.nightFrom || m < c.nightTo) : (m >= c.nightFrom && m < c.nightTo);
  }
  if (night == s_night) return;
  s_night = night;
  if (night && c.nightMode == 0) {
    s_oled.setPowerSave(1);
  } else {
    s_oled.setPowerSave(0);
    s_oled.setContrast(night ? 0 : c.displayContrast);
    render();
  }
  log_i("[%s] modo %s", TAG, night ? (c.nightMode == 0 ? "noturno (apagado)" : "noturno (escuro)") : "diurno");
}

void displayLoop() {
  if (!s_ok) return;
  uint32_t now = millis();

  if (s_splashUntil != 0) {
    if (splashActive()) {
      int frame = (now / DISPLAY_SPINNER_FRAME_MS) % SPIN_DOTS;
      if (frame != s_spinFrame) {
        s_spinFrame = frame;
        drawSpinner(frame);
        s_oled.updateDisplayArea(6, 1, 4, 4);
      }
      return;
    }
    // Fim da abertura: começa pela primeira página.
    s_splashUntil = 0;
    s_page = 0;
    s_pageSince = now;
    render();
    return;
  }

  // Abriu ou fechou a rede de configuração: redesenha na hora.
  static bool lastPortal = false;
  bool portal = netStatus().portalActive;
  if (portal != lastPortal) {
    lastPortal = portal;
    s_pageSince = now;
    render();
    return;
  }
  if (portal) return;  // tela fixa enquanto a configuração estiver aberta

  applyNight();
  if (s_night == 1 && settings().nightMode == 0) return;  // tela apagada: não precisa redesenhar

  if ((uint32_t)(now - s_pageSince) >= settings().displayPageMs) {
    s_pageSince = now;
    s_page = (s_page + 1) % pageCount();
    render();
  }
}

void displayApplySettings() {
  if (!s_ok) return;
  if (s_page >= pageCount()) s_page = 0;
  s_night = -1;  // reavalia o modo noturno com a nova configuração
  s_oled.setContrast(settings().displayContrast);
  applyNight();
  render();
}
