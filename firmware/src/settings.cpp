#include "settings.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "config.h"
#include "weather_math.h"

static const char *TAG = "settings";
static const char *PATH = "/config.json";

static Settings s_cfg;

static void applyDefaults(Settings &c) {
  c = Settings();
  char id[24];
  snprintf(id, sizeof id, "gweather-%06lx", (unsigned long)(esp_random() & 0xFFFFFF));
  c.deviceId = id;
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
}

static void fromJson(JsonDocument &doc, Settings &c) {
  // Campos ausentes mantêm o valor padrão já aplicado em c.
  c.deviceId = doc["device_id"] | c.deviceId;
  c.hostname = doc["hostname"] | c.hostname;
  c.tz = doc["time"]["tz"] | c.tz;
  c.ntp1 = doc["time"]["ntp1"] | c.ntp1;
  c.ntp2 = doc["time"]["ntp2"] | c.ntp2;
  c.groundAltM = doc["site"]["ground_alt_m"] | c.groundAltM;
  c.floor = doc["site"]["floor"] | c.floor;
  c.floorHeightM = doc["site"]["floor_height_m"] | c.floorHeightM;
  c.sensorHeightM = doc["site"]["sensor_height_m"] | c.sensorHeightM;
  c.displayPageMs = (doc["display"]["page_s"] | (int)(c.displayPageMs / 1000)) * 1000UL;
  c.displayContrast = doc["display"]["contrast"] | (int)c.displayContrast;
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
    if (v.is<JsonObjectConst>()) {
      if (!applyObject(c, v.as<JsonObjectConst>(), key, err)) return false;
      continue;
    }
    // Campos somente leitura são ignorados se vierem iguais ao atual.
    if (key == "schema" || key == "device_id") continue;
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
