#include "uplink.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "certs.h"
#include "config.h"
#include "netmgr.h"

static const char *TAG = "uplink";
static const char *QUEUE = "/queue.jsonl";
static const char *QUEUE_TMP = "/queue.tmp";

static UplinkStatus s_st;
static String s_token;
static uint32_t s_nextAttemptMs = 0;
static uint32_t s_backoffMs = 0;
static bool s_forceSend = false;

// Acumulador do intervalo atual de 10 min.
struct Bucket {
  uint32_t start = 0;  // epoch UTC do início do intervalo
  uint32_t nAht = 0, nBmp = 0;
  double temp = 0, hum = 0, dew = 0, pres = 0;
};
static Bucket s_bucket;

// --- fila em LittleFS ---

static uint32_t countLines() {
  File f = LittleFS.open(QUEUE, "r");
  if (!f) return 0;
  uint32_t n = 0;
  while (f.available())
    if (f.read() == '\n') n++;
  f.close();
  return n;
}

// Remove as primeiras n linhas (reescreve o arquivo sem elas).
static void dropLines(uint32_t n) {
  File in = LittleFS.open(QUEUE, "r");
  if (!in) return;
  File out = LittleFS.open(QUEUE_TMP, "w");
  uint32_t skipped = 0;
  while (in.available()) {
    String line = in.readStringUntil('\n');
    if (skipped < n) {
      skipped++;
      continue;
    }
    if (line.length()) out.println(line);
  }
  in.close();
  out.close();
  LittleFS.remove(QUEUE);
  LittleFS.rename(QUEUE_TMP, QUEUE);
  s_st.queue = countLines();
}

static void enqueue(const String &line) {
  if (s_st.queue >= UPLINK_QUEUE_MAX) {
    log_w("[%s] fila cheia (%u); descartando a média mais antiga", TAG, s_st.queue);
    dropLines(1);
  }
  File f = LittleFS.open(QUEUE, "a");
  if (!f) {
    log_e("[%s] não foi possível gravar a fila", TAG);
    return;
  }
  f.println(line);
  f.close();
  s_st.queue++;
}

static void finishBucket() {
  Bucket &b = s_bucket;
  // Exige pelo menos 1/4 do intervalo com dados.
  uint32_t minSamples = UPLINK_INTERVAL_S / (SENSOR_INTERVAL_MS / 1000) / 4;
  if (b.start && (b.nAht >= minSamples || b.nBmp >= minSamples)) {
    JsonDocument doc;
    doc["ts"] = b.start;
    if (b.nAht >= minSamples) {
      doc["temp"] = roundf(b.temp / b.nAht * 100) / 100;
      doc["hum"] = roundf(b.hum / b.nAht * 10) / 10;
      doc["dew"] = roundf(b.dew / b.nAht * 10) / 10;
    }
    if (b.nBmp >= minSamples) doc["pres_slp"] = roundf(b.pres / b.nBmp * 100) / 100;
    String line;
    serializeJson(doc, line);
    enqueue(line);
    log_i("[%s] média de 10 min na fila: %s (fila %u)", TAG, line.c_str(), s_st.queue);
    s_forceSend = true;  // tenta enviar logo
  }
  b = Bucket();
}

void uplinkAddReading(const Reading &r) {
  if (!netStatus().timeSynced) return;  // sem hora certa não dá para carimbar o intervalo
  uint32_t now = (uint32_t)time(nullptr);
  uint32_t start = now - now % UPLINK_INTERVAL_S;
  if (s_bucket.start != start) {
    if (s_bucket.start) finishBucket();
    s_bucket.start = start;
  }
  if (r.ahtOk) {
    s_bucket.nAht++;
    s_bucket.temp += r.tempC;
    s_bucket.hum += r.humPct;
    s_bucket.dew += r.dewPointC;
  }
  if (r.bmpOk) {
    s_bucket.nBmp++;
    s_bucket.pres += r.seaLevelHpa;
  }
}

// --- configuração (NVS) ---

static void loadConfig() {
  Preferences p;
  p.begin("gweather", true);
  s_st.url = p.getString("cloud_url", "");
  s_token = p.getString("cloud_token", "");
  p.end();
  s_st.configured = s_st.url.length() && s_token.length();
}

bool uplinkSetUrl(const String &url, String &err) {
  String u = url;
  while (u.endsWith("/")) u.remove(u.length() - 1);
  if (!u.startsWith("https://")) {
    err = "use um endereço https://";
    return false;
  }
  Preferences p;
  p.begin("gweather", false);
  p.putString("cloud_url", u);
  p.end();
  loadConfig();
  s_backoffMs = 0;
  return true;
}

bool uplinkSetToken(const String &token, String &err) {
  if (token.length() < 20 || token.length() > 100) {
    err = "token com tamanho inesperado";
    return false;
  }
  Preferences p;
  p.begin("gweather", false);
  p.putString("cloud_token", token);
  p.end();
  loadConfig();
  s_backoffMs = 0;
  return true;
}

void uplinkDisable() {
  Preferences p;
  p.begin("gweather", false);
  p.remove("cloud_url");
  p.remove("cloud_token");
  p.end();
  loadConfig();
}

void uplinkSendNow() {
  s_forceSend = true;
  s_backoffMs = 0;
  s_nextAttemptMs = 0;
}

uint32_t uplinkClearQueue() {
  uint32_t n = s_st.queue;
  LittleFS.remove(QUEUE);
  s_st.queue = 0;
  return n;
}

// --- envio ---

static void scheduleRetry() {
  s_backoffMs = s_backoffMs ? min<uint32_t>(s_backoffMs * 2, UPLINK_BACKOFF_MAX_MS) : 60000;
  s_nextAttemptMs = millis() + s_backoffMs;
}

static void sendBatch() {
  // Monta o lote com as médias mais antigas.
  File f = LittleFS.open(QUEUE, "r");
  if (!f) {
    s_st.queue = 0;
    return;
  }
  JsonDocument doc;
  JsonArray arr = doc["readings"].to<JsonArray>();
  uint32_t n = 0;
  while (f.available() && n < UPLINK_BATCH_MAX) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (!line.length()) continue;
    JsonDocument item;
    if (deserializeJson(item, line)) {
      n++;  // linha corrompida: conta para ser descartada junto
      continue;
    }
    arr.add(item);
    n++;
  }
  f.close();
  if (n == 0) return;

  JsonObject st = doc["status"].to<JsonObject>();
  st["fw"] = FW_VERSION;
  st["rssi"] = WiFi.RSSI();
  st["uptime_s"] = millis() / 1000;
  st["heap"] = ESP.getFreeHeap();
  st["queue"] = s_st.queue > n ? s_st.queue - n : 0;  // pendências depois deste lote
  String body;
  serializeJson(doc, body);

  WiFiClientSecure client;
  client.setCACert(CLOUD_ROOT_CAS);
  client.setTimeout(15);
  HTTPClient http;
  http.setTimeout(15000);
  String url = s_st.url + "/api/v1/ingest";
  if (!http.begin(client, url)) {
    s_st.lastHttp = -1;
    s_st.lastError = "URL inválida";
    scheduleRetry();
    return;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + s_token);
  int code = http.POST(body);
  String resp = http.getString();
  http.end();
  s_st.lastHttp = code;

  if (code == 200) {
    dropLines(n);
    s_st.lastOkEpoch = (uint32_t)time(nullptr);
    s_st.lastError = "";
    s_backoffMs = 0;
    s_nextAttemptMs = millis() + UPLINK_RETRY_MS;
    log_i("[%s] lote de %u enviado: %s (fila %u)", TAG, n, resp.c_str(), s_st.queue);
    if (s_st.queue) s_forceSend = true;  // ainda há pendências: continua
  } else if (code == 422 || code == 413) {
    // Lote recusado pelo conteúdo: descarta para não travar a fila.
    s_st.lastError = "lote recusado: " + resp.substring(0, 80);
    log_e("[%s] %d %s; descartando %u médias", TAG, code, resp.c_str(), n);
    dropLines(n);
    scheduleRetry();
  } else {
    s_st.lastError = code == 401 ? "token recusado" : (code < 0 ? http.errorToString(code) : "HTTP " + String(code));
    log_w("[%s] falha no envio (%d): %s", TAG, code, s_st.lastError.c_str());
    scheduleRetry();
  }
}

void uplinkBegin() {
  loadConfig();
  s_st.queue = countLines();
  s_forceSend = s_st.queue > 0;  // pendências de antes do reinício
  log_i("[%s] nuvem %s; fila com %u médias", TAG, s_st.configured ? s_st.url.c_str() : "não configurada", s_st.queue);
}

void uplinkLoop() {
  if (!s_st.configured || s_st.queue == 0) return;
  const NetStatus &n = netStatus();
  if (!n.connected || !n.timeSynced) return;  // TLS precisa da hora certa
  uint32_t now = millis();
  bool due = s_forceSend || (s_nextAttemptMs && (int32_t)(now - s_nextAttemptMs) >= 0);
  if (!due) return;
  if (s_forceSend && s_backoffMs && (int32_t)(now - s_nextAttemptMs) < 0) return;  // respeita o backoff
  s_forceSend = false;
  sendBatch();
}

const UplinkStatus &uplinkStatus() { return s_st; }
