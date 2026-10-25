#include "settings.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "config.h"
#include "weather_math.h"

static const char *TAG = "settings";
static const char *PATH = "/config.json";

static Settings s_cfg;
static uint32_t s_revision = 1;

static const char *PAGE_NAMES[PG_COUNT] = {"temp", "hum", "pres", "clock", "today", "fields", "details", "network", "rain"};
static const char *ALERT_MODES[4] = {"", "icon", "screen", "both"};
static const char *PAGE_MODES[3] = {"auto", "button", "both"};
static const char *LED_MODES[3] = {"normal", "warnings", "off"};
static const char *FIELD_NAMES[FD_COUNT] = {"temp", "hum", "pres", "dew", "heat_index", "temp_bmp", "pres_station"};

const char *pageTypeName(uint8_t t) { return t < PG_COUNT ? PAGE_NAMES[t] : "?"; }
const char *fieldTypeName(uint8_t f) { return f < FD_COUNT ? FIELD_NAMES[f] : "?"; }

static int indexOf(const char *const *names, int n, const char *s) {
  if (!s) return -1;
  for (int i = 0; i < n; i++)
    if (strcmp(names[i], s) == 0) return i;
  return -1;
}

static void defaultPages(Settings &c) {
  const uint8_t def[] = {PG_TEMP, PG_PRES, PG_DETAILS, PG_NETWORK};
  c.pageCount = sizeof def;
  for (int i = 0; i < c.pageCount; i++) {
    c.pages[i] = PageCfg();
    c.pages[i].type = def[i];
  }
}

static void applyDefaults(Settings &c) {
  String keepId = c.deviceId;
  c = Settings();
  if (keepId.length()) {
    c.deviceId = keepId;
  } else {
    char id[24];
    snprintf(id, sizeof id, "grain-%06lx", (unsigned long)(esp_random() & 0xFFFFFF));
    c.deviceId = id;
  }
  c.hostname = DEFAULT_HOSTNAME;
  c.tz = SITE_TZ;
  c.ntp1 = DEFAULT_NTP1;
  c.ntp2 = DEFAULT_NTP2;
  c.groundAltM = SITE_GROUND_ALT_M;
  c.floor = SITE_FLOOR;
  c.floorHeightM = SITE_FLOOR_HEIGHT_M;
  c.sensorHeightM = SITE_SENSOR_HEIGHT_M;
  c.displayPageMs = DISPLAY_PAGE_MS;
  c.displayContrast = DISPLAY_CONTRAST;
  defaultPages(c);
}

static String hhmm(uint16_t m) {
  char b[6];
  snprintf(b, sizeof b, "%02u:%02u", m / 60, m % 60);
  return b;
}

static bool parseHHMM(const String &s, uint16_t &out) {
  if (s.length() != 5 || s[2] != ':') return false;
  int h = s.substring(0, 2).toInt(), m = s.substring(3).toInt();
  if (!isdigit(s[0]) || !isdigit(s[1]) || !isdigit(s[3]) || !isdigit(s[4]) || h > 23 || m > 59) return false;
  out = h * 60 + m;
  return true;
}

// Lê a lista de páginas; tudo ou nada.
static bool parsePages(Settings &c, JsonArrayConst arr, String &err) {
  if (arr.size() < 1 || arr.size() > MAX_PAGES) {
    err = "de 1 a 8 páginas";
    return false;
  }
  PageCfg tmp[MAX_PAGES];
  int n = 0;
  for (JsonVariantConst p : arr) {
    int t = indexOf(PAGE_NAMES, PG_COUNT, p["type"] | (const char *)nullptr);
    if (t < 0) {
      err = "tipo de página desconhecido";
      return false;
    }
    PageCfg pc;
    pc.type = t;
    if (t == PG_FIELDS) {
      JsonArrayConst fs = p["fields"];
      if (fs.isNull() || fs.size() < 1 || fs.size() > MAX_FIELDS) {
        err = "a página Resumo precisa de 1 a 4 campos";
        return false;
      }
      for (JsonVariantConst f : fs) {
        int fi = indexOf(FIELD_NAMES, FD_COUNT, f.as<const char *>());
        if (fi < 0) {
          err = "campo desconhecido";
          return false;
        }
        pc.fields[pc.nfields++] = fi;
      }
    }
    tmp[n++] = pc;
  }
  for (int i = 0; i < n; i++) c.pages[i] = tmp[i];
  c.pageCount = n;
  return true;
}

static void toJson(const Settings &c, JsonDocument &doc) {
  doc["schema"] = c.schema;
  doc["device_id"] = c.deviceId;
  doc["hostname"] = c.hostname;
  JsonObject time = doc["time"].to<JsonObject>();
  time["tz"] = c.tz;
  time["ntp1"] = c.ntp1;
  time["ntp2"] = c.ntp2;
  JsonObject site = doc["site"].to<JsonObject>();
  site["ground_alt_m"] = c.groundAltM;
  site["floor"] = c.floor;
  site["floor_height_m"] = c.floorHeightM;
  site["sensor_height_m"] = c.sensorHeightM;
  JsonObject disp = doc["display"].to<JsonObject>();
  disp["page_s"] = c.displayPageMs / 1000;
  disp["contrast"] = c.displayContrast;
  JsonObject night = disp["night"].to<JsonObject>();
  night["enabled"] = c.nightEnabled;
  night["from"] = hhmm(c.nightFrom);
  night["to"] = hhmm(c.nightTo);
  night["mode"] = c.nightMode == 0 ? "off" : "dim";
  disp["page_mode"] = PAGE_MODES[c.pageMode % 3];
  disp["idle_dim_min"] = c.idleDimMin;
  JsonArray pages = disp["pages"].to<JsonArray>();
  for (int i = 0; i < c.pageCount; i++) {
    JsonObject p = pages.add<JsonObject>();
    p["type"] = pageTypeName(c.pages[i].type);
    if (c.pages[i].type == PG_FIELDS) {
      JsonArray fs = p["fields"].to<JsonArray>();
      for (int k = 0; k < c.pages[i].nfields; k++) fs.add(fieldTypeName(c.pages[i].fields[k]));
    }
  }
  JsonObject cal = doc["calibration"].to<JsonObject>();
  cal["temp"] = c.calTemp;
  cal["hum"] = c.calHum;
  cal["pres"] = c.calPres;
  cal["pres_auto"] = c.presAuto;
  cal["pres_auto_offset"] = roundf(c.presAutoOffset * 100) / 100;
  JsonObject ra = doc["rain_alert"].to<JsonObject>();
  ra["enabled"] = c.rainAlert;
  ra["threshold"] = c.rainAlertPct;
  ra["hours"] = c.rainAlertHours;
  ra["mode"] = ALERT_MODES[c.rainAlertMode & 3];
  JsonObject bz = doc["buzzer"].to<JsonObject>();
  bz["enabled"] = c.buzzerEnabled;
  bz["clicks"] = c.buzzerClicks;
  bz["level"] = c.buzzerActiveLow ? "low" : "high";
  JsonObject led = doc["led"].to<JsonObject>();
  led["mode"] = LED_MODES[c.ledMode % 3];
  led["quiet"] = LED_MODES[c.ledQuietMode % 3];
  JsonObject quiet = doc["quiet"].to<JsonObject>();
  quiet["enabled"] = c.quietEnabled;
  quiet["from"] = hhmm(c.quietFrom);
  quiet["to"] = hhmm(c.quietTo);
  JsonObject loc = doc["location"].to<JsonObject>();
  if (isnan(c.lat) || isnan(c.lon)) {
    loc["lat"] = nullptr;
    loc["lon"] = nullptr;
  } else {
    loc["lat"] = c.lat;
    loc["lon"] = c.lon;
  }
  loc["label"] = c.placeLabel;
}

static void fromJson(JsonDocument &doc, Settings &c) {
  // Campos ausentes mantêm o valor padrão já aplicado em c.
  c.deviceId = doc["device_id"] | c.deviceId;
  c.hostname = doc["hostname"] | c.hostname;
  if (c.hostname == LEGACY_HOSTNAME) c.hostname = DEFAULT_HOSTNAME;  // projeto renomeado para GRain
  c.tz = doc["time"]["tz"] | c.tz;
  c.ntp1 = doc["time"]["ntp1"] | c.ntp1;
  c.ntp2 = doc["time"]["ntp2"] | c.ntp2;
  c.groundAltM = doc["site"]["ground_alt_m"] | c.groundAltM;
  c.floor = doc["site"]["floor"] | c.floor;
  c.floorHeightM = doc["site"]["floor_height_m"] | c.floorHeightM;
  c.sensorHeightM = doc["site"]["sensor_height_m"] | c.sensorHeightM;
  c.displayPageMs = (doc["display"]["page_s"] | (int)(c.displayPageMs / 1000)) * 1000UL;
  c.displayContrast = doc["display"]["contrast"] | (int)c.displayContrast;
  JsonObject night = doc["display"]["night"];
  if (!night.isNull()) {
    c.nightEnabled = night["enabled"] | c.nightEnabled;
    parseHHMM(night["from"] | "", c.nightFrom);
    parseHHMM(night["to"] | "", c.nightTo);
    c.nightMode = strcmp(night["mode"] | "dim", "off") == 0 ? 0 : 1;
  }
  int pm = indexOf(PAGE_MODES, 3, doc["display"]["page_mode"] | "");
  if (pm >= 0) c.pageMode = pm;
  c.idleDimMin = doc["display"]["idle_dim_min"] | (int)c.idleDimMin;
  c.buzzerEnabled = doc["buzzer"]["enabled"] | c.buzzerEnabled;
  c.buzzerClicks = doc["buzzer"]["clicks"] | c.buzzerClicks;
  c.buzzerActiveLow = strcmp(doc["buzzer"]["level"] | (c.buzzerActiveLow ? "low" : "high"), "low") == 0;
  int lm = indexOf(LED_MODES, 3, doc["led"]["mode"] | "");
  if (lm >= 0) c.ledMode = lm;
  lm = indexOf(LED_MODES, 3, doc["led"]["quiet"] | "");
  if (lm >= 0) c.ledQuietMode = lm;
  c.quietEnabled = doc["quiet"]["enabled"] | c.quietEnabled;
  parseHHMM(doc["quiet"]["from"] | "", c.quietFrom);
  parseHHMM(doc["quiet"]["to"] | "", c.quietTo);
  JsonArrayConst pages = doc["display"]["pages"];
  String err;
  if (!pages.isNull() && !parsePages(c, pages, err)) log_w("[%s] páginas inválidas (%s); usando padrão", TAG, err.c_str());
  c.calTemp = doc["calibration"]["temp"] | c.calTemp;
  c.calHum = doc["calibration"]["hum"] | c.calHum;
  c.calPres = doc["calibration"]["pres"] | c.calPres;
  c.presAuto = doc["calibration"]["pres_auto"] | c.presAuto;
  c.presAutoOffset = doc["calibration"]["pres_auto_offset"] | c.presAutoOffset;
  c.rainAlert = doc["rain_alert"]["enabled"] | c.rainAlert;
  c.rainAlertPct = doc["rain_alert"]["threshold"] | (int)c.rainAlertPct;
  c.rainAlertHours = doc["rain_alert"]["hours"] | (int)c.rainAlertHours;
  int mode = indexOf(ALERT_MODES, 4, doc["rain_alert"]["mode"] | "both");
  if (mode > 0) c.rainAlertMode = mode;
  c.lat = doc["location"]["lat"] | c.lat;
  c.lon = doc["location"]["lon"] | c.lon;
  c.placeLabel = doc["location"]["label"] | c.placeLabel;
}

static bool save() {
  JsonDocument doc;
  toJson(s_cfg, doc);
  File f = LittleFS.open(PATH, "w");
  if (!f) {
    log_e("[%s] não foi possível gravar %s", TAG, PATH);
    return false;
  }
  serializeJsonPretty(doc, f);
  f.close();
  s_revision++;
  return true;
}

bool settingsBegin() {
  applyDefaults(s_cfg);
  if (!LittleFS.begin(true)) {  // formata na primeira vez
    log_e("[%s] LittleFS indisponível; usando padrões", TAG);
    return false;
  }
  File f = LittleFS.open(PATH, "r");
  if (!f) {
    log_i("[%s] %s não existe; criando com padrões", TAG, PATH);
    return save();
  }
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, f);
  f.close();
  if (e) {
    log_w("[%s] %s inválido (%s); recriando com padrões", TAG, PATH, e.c_str());
    return save();
  }
  fromJson(doc, s_cfg);
  if ((doc["schema"] | 1) < s_cfg.schema) save();  // grava os campos novos (e descarta os que saíram)
  log_i("[%s] configuração carregada de %s", TAG, PATH);
  return true;
}

const Settings &settings() { return s_cfg; }

float settingsAltitudeM() {
  return sensorAltitudeM(s_cfg.groundAltM, s_cfg.floor, s_cfg.floorHeightM, s_cfg.sensorHeightM);
}

static bool parseFloat(const String &v, float lo, float hi, float &out) {
  char *end;
  float f = strtof(v.c_str(), &end);
  if (end == v.c_str() || *end != '\0' || f < lo || f > hi) return false;
  out = f;
  return true;
}

// Valida e aplica um campo em c (sem gravar).
static bool setField(Settings &c, const String &key, const String &value, String &err) {
  float f;
  uint16_t m;
  if (key == "hostname") {
    if (value.isEmpty() || value.length() > 31) { err = "use 1 a 31 caracteres"; return false; }
    for (char ch : value)
      if (!isalnum((unsigned char)ch) && ch != '-') { err = "use só letras, números e hífen"; return false; }
    c.hostname = value;
  } else if (key == "time.tz") {
    if (value.isEmpty()) { err = "fuso vazio"; return false; }
    c.tz = value;
  } else if (key == "time.ntp1") {
    c.ntp1 = value;
  } else if (key == "time.ntp2") {
    c.ntp2 = value;
  } else if (key == "site.ground_alt_m") {
    if (!parseFloat(value, -500, 6000, f)) { err = "entre -500 e 6000"; return false; }
    c.groundAltM = f;
  } else if (key == "site.floor") {
    if (!parseFloat(value, 0, 200, f)) { err = "entre 0 e 200"; return false; }
    c.floor = (int)f;
  } else if (key == "site.floor_height_m") {
    if (!parseFloat(value, 2, 6, f)) { err = "entre 2 e 6"; return false; }
    c.floorHeightM = f;
  } else if (key == "site.sensor_height_m") {
    if (!parseFloat(value, 0, 50, f)) { err = "entre 0 e 50"; return false; }
    c.sensorHeightM = f;
  } else if (key == "display.page_s") {
    if (!parseFloat(value, 2, 60, f)) { err = "entre 2 e 60"; return false; }
    c.displayPageMs = (uint32_t)f * 1000;
  } else if (key == "display.contrast") {
    if (!parseFloat(value, 0, 255, f)) { err = "entre 0 e 255"; return false; }
    c.displayContrast = (uint8_t)f;
  } else if (key == "display.night.enabled") {
    if (value != "true" && value != "false") { err = "true ou false"; return false; }
    c.nightEnabled = value == "true";
  } else if (key == "display.night.from" || key == "display.night.to") {
    if (!parseHHMM(value, m)) { err = "formato HH:MM"; return false; }
    (key.endsWith("from") ? c.nightFrom : c.nightTo) = m;
  } else if (key == "display.night.mode") {
    if (value != "off" && value != "dim") { err = "off ou dim"; return false; }
    c.nightMode = value == "off" ? 0 : 1;
  } else if (key == "display.page_mode") {
    int pm = indexOf(PAGE_MODES, 3, value.c_str());
    if (pm < 0) { err = "auto, button ou both"; return false; }
    c.pageMode = pm;
  } else if (key == "display.idle_dim_min") {
    if (!parseFloat(value, 0, 120, f)) { err = "entre 0 e 120"; return false; }
    c.idleDimMin = (uint8_t)f;
  } else if (key == "buzzer.enabled" || key == "buzzer.clicks" || key == "quiet.enabled") {
    if (value != "true" && value != "false") { err = "true ou false"; return false; }
    (key == "buzzer.enabled" ? c.buzzerEnabled : key == "buzzer.clicks" ? c.buzzerClicks : c.quietEnabled) = value == "true";
  } else if (key == "buzzer.level") {
    if (value != "high" && value != "low") { err = "high ou low"; return false; }
    c.buzzerActiveLow = value == "low";
  } else if (key == "quiet.from" || key == "quiet.to") {
    if (!parseHHMM(value, m)) { err = "formato HH:MM"; return false; }
    (key.endsWith("from") ? c.quietFrom : c.quietTo) = m;
  } else if (key == "led.mode" || key == "led.quiet") {
    int lm = indexOf(LED_MODES, 3, value.c_str());
    if (lm < 0) { err = "normal, warnings ou off"; return false; }
    (key == "led.mode" ? c.ledMode : c.ledQuietMode) = lm;
  } else if (key == "calibration.temp") {
    if (!parseFloat(value, -5, 5, f)) { err = "entre -5 e 5"; return false; }
    c.calTemp = f;
  } else if (key == "calibration.hum") {
    if (!parseFloat(value, -15, 15, f)) { err = "entre -15 e 15"; return false; }
    c.calHum = f;
  } else if (key == "calibration.pres") {
    if (!parseFloat(value, -15, 15, f)) { err = "entre -15 e 15"; return false; }
    c.calPres = f;
  } else if (key == "calibration.pres_auto") {
    if (value != "true" && value != "false") { err = "true ou false"; return false; }
    c.presAuto = value == "true";
  } else if (key == "calibration.pres_auto_offset") {
    if (!parseFloat(value, -15, 15, f)) { err = "entre -15 e 15"; return false; }
    c.presAutoOffset = f;
  } else if (key == "rain_alert.enabled") {
    if (value != "true" && value != "false") { err = "true ou false"; return false; }
    c.rainAlert = value == "true";
  } else if (key == "rain_alert.threshold") {
    if (!parseFloat(value, 10, 100, f)) { err = "entre 10 e 100"; return false; }
    c.rainAlertPct = (uint8_t)f;
  } else if (key == "rain_alert.hours") {
    if (!parseFloat(value, 1, 3, f)) { err = "1, 2 ou 3"; return false; }
    c.rainAlertHours = (uint8_t)f;
  } else if (key == "rain_alert.mode") {
    int m = indexOf(ALERT_MODES, 4, value.c_str());
    if (m <= 0) { err = "icon, screen ou both"; return false; }
    c.rainAlertMode = m;
  } else if (key == "location.lat" || key == "location.lon") {
    bool isLat = key.endsWith("lat");
    if (value.isEmpty() || value == "null") {
      (isLat ? c.lat : c.lon) = NAN;
    } else if (!parseFloat(value, isLat ? -90 : -180, isLat ? 90 : 180, f)) {
      err = isLat ? "latitude entre -90 e 90" : "longitude entre -180 e 180";
      return false;
    } else {
      (isLat ? c.lat : c.lon) = f;
    }
  } else if (key == "location.label") {
    if (value.length() > 40) { err = "até 40 caracteres"; return false; }
    c.placeLabel = value == "null" ? "" : value;
  } else {
    err = "chave desconhecida";
    return false;
  }
  return true;
}

bool settingsSet(const String &key, const String &value, String &err) {
  Settings c = s_cfg;
  if (!setField(c, key, value, err)) return false;
  s_cfg = c;
  return save();
}

// Percorre o JSON e transforma cada folha em "secao.chave" = valor.
static bool applyObject(Settings &c, JsonObjectConst obj, const String &prefix, String &err) {
  for (JsonPairConst kv : obj) {
    String key = prefix.isEmpty() ? String(kv.key().c_str()) : prefix + "." + kv.key().c_str();
    JsonVariantConst v = kv.value();
    // Campos somente leitura são ignorados.
    if (key == "schema" || key == "device_id" || key.startsWith("cloud")) continue;
    if (key == "display.pages") {
      String e;
      if (!v.is<JsonArrayConst>() || !parsePages(c, v.as<JsonArrayConst>(), e)) {
        err = key + ": " + (e.length() ? e : String("esperada uma lista"));
        return false;
      }
      continue;
    }
    if (v.is<JsonObjectConst>()) {
      if (!applyObject(c, v.as<JsonObjectConst>(), key, err)) return false;
      continue;
    }
    String value;
    if (v.is<const char *>()) value = v.as<const char *>();
    else serializeJson(v, value);
    String e;
    if (!setField(c, key, value, e)) {
      err = key + ": " + e;
      return false;
    }
  }
  return true;
}

bool settingsApplyJson(JsonObjectConst obj, String &err) {
  Settings c = s_cfg;
  if (!applyObject(c, obj, "", err)) return false;
  s_cfg = c;
  if (!save()) {
    err = "falha ao gravar";
    return false;
  }
  return true;
}

uint32_t settingsRevision() { return s_revision; }

void settingsCloudCopy(JsonDocument &doc) {
  JsonDocument all;
  toJson(s_cfg, all);
  doc["display"] = all["display"];
  doc["calibration"] = all["calibration"];
  doc["location"] = all["location"];
  doc["rain_alert"] = all["rain_alert"];
  doc["buzzer"] = all["buzzer"];
  doc["led"] = all["led"];
  doc["quiet"] = all["quiet"];
}

void settingsToJson(JsonDocument &doc) { toJson(s_cfg, doc); }

bool settingsReset() {
  applyDefaults(s_cfg);
  return save();
}

void settingsPrint() {
  JsonDocument doc;
  toJson(s_cfg, doc);
  serializeJsonPretty(doc, Serial);
  Serial.println();
}
