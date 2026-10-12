#include "webapi.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <time.h>

#include "config.h"
#include "display.h"
#include "netmgr.h"
#include "sensors.h"
#include "settings.h"
#include "uplink.h"
#include "webpage.h"

static const char *TAG = "webapi";

static WebServer s_srv(80);
static bool s_running = false;
static bool s_routesReady = false;
static uint32_t s_rebootAt = 0;

// --- utilitários ---

static void cors() {
  s_srv.sendHeader("Access-Control-Allow-Origin", "*");
  s_srv.sendHeader("Access-Control-Allow-Methods", "GET, PUT, POST, OPTIONS");
  s_srv.sendHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");
}

static void sendJson(int code, JsonDocument &doc) {
  String body;
  serializeJson(doc, body);
  cors();
  s_srv.sendHeader("Cache-Control", "no-store");
  s_srv.send(code, "application/json; charset=utf-8", body);
}

static void sendError(int code, const char *error, const String &detail = "") {
  JsonDocument doc;
  doc["error"] = error;
  if (!detail.isEmpty()) doc["detail"] = detail;
  sendJson(code, doc);
}

// Rotas que alteram algo exigem usuário "admin" e a senha da estação
// (a mesma da rede de configuração, mostrada pelo comando "status").
static bool requireAuth() {
  if (s_srv.authenticate(API_USER, netStatus().apPass.c_str())) return true;
  cors();
  s_srv.requestAuthentication(BASIC_AUTH, "GWeather");
  return false;
}

static void setOrNull(JsonDocument &doc, const char *key, bool ok, float v, int decimals) {
  if (!ok || isnan(v)) {
    doc[key] = nullptr;
    return;
  }
  float p = powf(10, decimals);
  doc[key] = roundf(v * p) / p;
}

// --- rotas ---

static void handleIndex() {
  s_srv.sendHeader("Cache-Control", "no-store");
  s_srv.send_P(200, "text/html; charset=utf-8", WEB_INDEX_HTML);
}

static void handleNow() {
  const Reading &r = sensorsLast();
  JsonDocument doc;
  doc["device_id"] = settings().deviceId;
  time_t t = time(nullptr);
  if (netStatus().timeSynced) doc["ts"] = (uint32_t)t;
  else doc["ts"] = nullptr;
  uint32_t lastMs = sensorsLastMs();
  doc["age_s"] = lastMs ? (millis() - lastMs) / 1000 : 0;
  setOrNull(doc, "temp", r.ahtOk, r.tempC, 2);
  setOrNull(doc, "hum", r.ahtOk, r.humPct, 1);
  setOrNull(doc, "dew", r.ahtOk, r.dewPointC, 1);
  setOrNull(doc, "heat_index", r.ahtOk, r.heatIndexC, 1);
  setOrNull(doc, "temp_bmp", r.bmpOk, r.tempBmpC, 2);
  setOrNull(doc, "pres_station", r.bmpOk, r.stationHpa, 2);
  setOrNull(doc, "pres_slp", r.bmpOk, r.seaLevelHpa, 2);
  doc["altitude_m"] = roundf(sensorsAltitude() * 10) / 10;
  sendJson(lastMs ? 200 : 503, doc);
}

static void handleStatus() {
  const NetStatus &n = netStatus();
  JsonDocument doc;
  JsonObject dev = doc["device"].to<JsonObject>();
  dev["id"] = settings().deviceId;
  dev["firmware"] = FW_VERSION;
  dev["uptime_s"] = millis() / 1000;
  dev["heap_free"] = ESP.getFreeHeap();
  dev["heap_min"] = ESP.getMinFreeHeap();
  dev["fs_used"] = LittleFS.usedBytes();
  dev["fs_total"] = LittleFS.totalBytes();
  JsonObject wifi = doc["wifi"].to<JsonObject>();
  wifi["connected"] = n.connected;
  wifi["ssid"] = n.ssid;
  wifi["ip"] = n.ip;
  wifi["rssi"] = n.rssi;
  wifi["hostname"] = settings().hostname + ".local";
  JsonObject tm = doc["time"].to<JsonObject>();
  tm["synced"] = n.timeSynced;
  tm["tz"] = settings().tz;
  char buf[32];
  if (netLocalTime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%S")) {
    tm["local"] = buf;
    tm["epoch"] = (uint32_t)time(nullptr);
  } else {
    tm["local"] = nullptr;
    tm["epoch"] = nullptr;
  }
  const UplinkStatus &u = uplinkStatus();
  JsonObject cloud = doc["cloud"].to<JsonObject>();
  cloud["configured"] = u.configured;
  cloud["url"] = u.url;
  cloud["queue"] = u.queue;
  if (u.lastOkEpoch) cloud["last_ok"] = u.lastOkEpoch;
  else cloud["last_ok"] = nullptr;
  cloud["last_http"] = u.lastHttp;
  cloud["last_error"] = u.lastError;
  JsonObject s = doc["sensors"].to<JsonObject>();
  s["aht20"] = sensorsAhtPresent() && sensorsLast().ahtOk;
  s["bmp280"] = sensorsBmpPresent() && sensorsLast().bmpOk;
  sendJson(200, doc);
}

static void handleGetConfig() {
  JsonDocument doc;
  settingsToJson(doc);
  sendJson(200, doc);
}

static void handlePutConfig() {
  if (!requireAuth()) return;
  if (s_srv.header("Content-Type").indexOf("application/json") < 0)
    return sendError(415, "tipo_invalido", "use o cabeçalho Content-Type: application/json");
  if (!s_srv.hasArg("plain")) return sendError(400, "corpo_vazio", "envie um JSON");
  JsonDocument in;
  DeserializationError e = deserializeJson(in, s_srv.arg("plain"));
  if (e) return sendError(400, "json_invalido", e.c_str());
  if (!in.is<JsonObject>()) return sendError(400, "json_invalido", "esperado um objeto");
  String err;
  if (!settingsApplyJson(in.as<JsonObjectConst>(), err)) return sendError(422, "valor_invalido", err);
  sensorsSetAltitude(settingsAltitudeM());
  displayApplySettings();
  log_i("[%s] configuração alterada pela API", TAG);
  JsonDocument out;
  settingsToJson(out);
  sendJson(200, out);
}

static void handleReboot() {
  if (!requireAuth()) return;
  JsonDocument doc;
  doc["ok"] = true;
  doc["detail"] = "reiniciando";
  sendJson(202, doc);
  s_rebootAt = millis() + 500;
  if (s_rebootAt == 0) s_rebootAt = 1;
}

static void handleOptions() {
  cors();
  s_srv.send(204);
}

static void handleNotFound() {
  if (s_srv.method() == HTTP_OPTIONS) return handleOptions();
  sendError(404, "nao_encontrado", s_srv.uri());
}

static void setupRoutes() {
  s_srv.on("/", HTTP_GET, handleIndex);
  s_srv.on("/api/v1/now", HTTP_GET, handleNow);
  s_srv.on("/api/v1/status", HTTP_GET, handleStatus);
  s_srv.on("/api/v1/config", HTTP_GET, handleGetConfig);
  s_srv.on("/api/v1/config", HTTP_PUT, handlePutConfig);
  s_srv.on("/api/v1/system/reboot", HTTP_POST, handleReboot);
  s_srv.onNotFound(handleNotFound);
  const char *headers[] = {"Content-Type"};
  s_srv.collectHeaders(headers, 1);
  s_routesReady = true;
}

void webapiLoop() {
  const NetStatus &n = netStatus();
  bool shouldRun = n.connected && !n.portalActive;

  if (shouldRun && !s_running) {
    if (!s_routesReady) setupRoutes();
    s_srv.begin();
    static bool mdnsService = false;
    if (!mdnsService) mdnsService = MDNS.addService("http", "tcp", 80);
    s_running = true;
    log_i("[%s] API em http://%s.local/api/v1 (http://%s)", TAG, settings().hostname.c_str(), n.ip.c_str());
  } else if (!shouldRun && s_running) {
    s_srv.stop();
    s_running = false;
    log_i("[%s] servidor parado (sem Wi-Fi ou rede de configuração aberta)", TAG);
  }

  if (s_running) s_srv.handleClient();

  if (s_rebootAt && (int32_t)(millis() - s_rebootAt) >= 0) {
    log_w("[%s] reiniciando a pedido da API", TAG);
    ESP.restart();
  }
}
