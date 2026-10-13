#include "settings.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "config.h"
#include "weather_math.h"

static const char *TAG = "settings";
static const char *PATH = "/config.json";

static Settings s_cfg;

static const char *PAGE_NAMES[PG_COUNT] = {"temp", "hum", "pres", "clock", "today", "fields", "details", "network"};
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
  doc["cloud"]["config_version"] = c.cloudConfigVersion;
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
  JsonArrayConst pages = doc["display"]["pages"];
  String err;
  if (!pages.isNull() && !parsePages(c, pages, err)) log_w("[%s] páginas inválidas (%s); usando padrão", TAG, err.c_str());
  c.calTemp = doc["calibration"]["temp"] | c.calTemp;
  c.calHum = doc["calibration"]["hum"] | c.calHum;
  c.calPres = doc["calibration"]["pres"] | c.calPres;
  c.cloudConfigVersion = doc["cloud"]["config_version"] | c.cloudConfigVersion;
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
  if ((doc["schema"] | 1) < s_cfg.schema) save();  // grava os campos novos
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
  } else if (key == "calibration.temp") {
    if (!parseFloat(value, -5, 5, f)) { err = "entre -5 e 5"; return false; }
    c.calTemp = f;
  } else if (key == "calibration.hum") {
    if (!parseFloat(value, -15, 15, f)) { err = "entre -15 e 15"; return false; }
    c.calHum = f;
  } else if (key == "calibration.pres") {
    if (!parseFloat(value, -15, 15, f)) { err = "entre -15 e 15"; return false; }
    c.calPres = f;
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

bool settingsApplyCloud(JsonObjectConst config, uint32_t version, String &err) {
  // A nuvem só controla display e calibração; o resto continua local.
  JsonDocument filtered;
  if (!config["display"].isNull()) filtered["display"] = config["display"];
  if (!config["calibration"].isNull()) filtered["calibration"] = config["calibration"];
  Settings c = s_cfg;
  if (!applyObject(c, filtered.as<JsonObjectConst>(), "", err)) return false;
  c.cloudConfigVersion = version;
  s_cfg = c;
  if (!save()) {
    err = "falha ao gravar";
    return false;
  }
  return true;
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
