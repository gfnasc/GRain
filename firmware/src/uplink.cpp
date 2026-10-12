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
#include "display.h"
#include "netmgr.h"
#include "settings.h"

static const char *TAG = "uplink";
static const char *QUEUE = "/queue.jsonl";
static const char *QUEUE_TMP = "/queue.tmp";

// Tudo abaixo é compartilhado entre o loop principal e a tarefa de rede:
// acesso sempre com s_lock.
static SemaphoreHandle_t s_lock;
static UplinkStatus s_st;
static String s_token;
static uint32_t s_nextAttemptMs = 0;
static uint32_t s_backoffMs = 0;
static bool s_forceSend = false;
static bool s_forcePoll = false;
static String s_pendingConfig;     // corpo recebido da nuvem, a aplicar no loop principal
static uint32_t s_pendingVersion = 0;

struct Lock {
  Lock() { xSemaphoreTake(s_lock, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(s_lock); }
};

// Acumulador do intervalo atual de 10 min (só no loop principal).
struct Bucket {
  uint32_t start = 0;  // epoch UTC do início do intervalo
  uint32_t nAht = 0, nBmp = 0;
  double temp = 0, hum = 0, dew = 0, pres = 0;
};
static Bucket s_bucket;

// --- fila em LittleFS (chamar com s_lock) ---

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
  Lock l;
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
  s_forceSend = true;  // tenta enviar logo
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
    log_i("[%s] média de 10 min na fila: %s", TAG, line.c_str());
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
  String url = p.getString("cloud_url", "");
  String tok = p.getString("cloud_token", "");
  p.end();
  Lock l;
  s_st.url = url;
  s_token = tok;
  s_st.configured = url.length() && tok.length();
  s_backoffMs = 0;
  s_forcePoll = true;
}

bool uplinkSetUrl(const String &url, String &err) {
  String u = url;
  u.trim();
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
  Lock l;
  s_forceSend = true;
  s_forcePoll = true;
  s_backoffMs = 0;
  s_nextAttemptMs = 0;
}

uint32_t uplinkClearQueue() {
  Lock l;
  uint32_t n = s_st.queue;
  LittleFS.remove(QUEUE);
  s_st.queue = 0;
  return n;
}

UplinkStatus uplinkStatus() {
  Lock l;
  return s_st;
}

// --- tarefa de rede ---

// Faz uma requisição HTTPS. Roda só na tarefa de rede.
static int https(const char *method, const String &url, const String &token, const String &body, String &resp) {
  WiFiClientSecure client;
  client.setCACert(CLOUD_ROOT_CAS);
  client.setTimeout(15);
  HTTPClient http;
  http.setTimeout(15000);
  if (!http.begin(client, url)) return -1;
  http.addHeader("Authorization", "Bearer " + token);
  int code;
  if (strcmp(method, "POST") == 0) {
    http.addHeader("Content-Type", "application/json");
    code = http.POST(body);
  } else {
    code = http.GET();
  }
  resp = code > 0 ? http.getString() : http.errorToString(code);
  http.end();
  return code;
}

static void scheduleRetry() {
  s_backoffMs = s_backoffMs ? min<uint32_t>(s_backoffMs * 2, UPLINK_BACKOFF_MAX_MS) : 60000;
  s_nextAttemptMs = millis() + s_backoffMs;
}

static void sendBatch() {
  String url, token, body;
  uint32_t n = 0;
  {
    Lock l;
    url = s_st.url + "/api/v1/ingest";
    token = s_token;
    File f = LittleFS.open(QUEUE, "r");
    if (!f) {
      s_st.queue = 0;
      return;
    }
    JsonDocument doc;
    JsonArray arr = doc["readings"].to<JsonArray>();
    while (f.available() && n < UPLINK_BATCH_MAX) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (!line.length()) continue;
      JsonDocument item;
      n++;  // linha corrompida também conta, para ser descartada junto
      if (!deserializeJson(item, line)) arr.add(item);
    }
    f.close();
    if (n == 0) return;
    JsonObject st = doc["status"].to<JsonObject>();
    st["fw"] = FW_VERSION;
    st["rssi"] = WiFi.RSSI();
    st["uptime_s"] = millis() / 1000;
    st["heap"] = ESP.getFreeHeap();
    st["queue"] = s_st.queue > n ? s_st.queue - n : 0;  // pendências depois deste lote
    st["config_v"] = s_st.configVersion;
    serializeJson(doc, body);
  }

  String resp;
  int code = https("POST", url, token, body, resp);

  Lock l;
  s_st.lastHttp = code;
  if (code == 200) {
    dropLines(n);
    s_st.lastOkEpoch = (uint32_t)time(nullptr);
    s_st.lastError = "";
    s_backoffMs = 0;
    s_nextAttemptMs = 0;
    log_i("[%s] lote de %u enviado: %s (fila %u)", TAG, n, resp.c_str(), s_st.queue);
    if (s_st.queue) s_forceSend = true;  // ainda há pendências: continua
  } else if (code == 422 || code == 413) {
    // Lote recusado pelo conteúdo: descarta para não travar a fila.
    s_st.lastError = "lote recusado: " + resp.substring(0, 80);
    log_e("[%s] %d %s; descartando %u médias", TAG, code, resp.c_str(), n);
    dropLines(n);
    scheduleRetry();
  } else {
    s_st.lastError = code == 401 ? "token recusado" : (code < 0 ? resp : "HTTP " + String(code));
    log_w("[%s] falha no envio (%d): %s", TAG, code, s_st.lastError.c_str());
    scheduleRetry();
  }
}

static void pollConfig() {
  String url, token;
  {
    Lock l;
    url = s_st.url + "/api/v1/device/config?v=" + String(s_st.configVersion);
    token = s_token;
  }
  String resp;
  int code = https("GET", url, token, "", resp);
  Lock l;
  if (code == 204 || code == 200) s_st.lastConfigEpoch = (uint32_t)time(nullptr);
  if (code == 200) {
    JsonDocument doc;
    if (deserializeJson(doc, resp) || !doc["version"].is<uint32_t>()) {
      log_w("[%s] configuração da nuvem ilegível", TAG);
      return;
    }
    s_pendingVersion = doc["version"];
    s_pendingConfig = resp;
    log_i("[%s] nova configuração da nuvem: versão %u", TAG, s_pendingVersion);
  } else if (code != 204) {
    log_w("[%s] consulta de configuração falhou (%d)", TAG, code);
  }
}

static void netTask(void *) {
  uint32_t lastPoll = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(500));
    const NetStatus &n = netStatus();
    bool configured, sendDue, pollDue;
    {
      Lock l;
      configured = s_st.configured;
      uint32_t now = millis();
      bool backoffOk = !s_backoffMs || (int32_t)(now - s_nextAttemptMs) >= 0;
      sendDue = s_st.queue > 0 && backoffOk && (s_forceSend || s_backoffMs);
      pollDue = s_forcePoll || (uint32_t)(now - lastPoll) >= CLOUD_CONFIG_POLL_MS;
    }
    if (!configured || !n.connected || !n.timeSynced) continue;  // TLS precisa da hora certa
    if (sendDue) {
      {
        Lock l;
        s_forceSend = false;
      }
      sendBatch();
    }
    if (pollDue) {
      {
        Lock l;
        s_forcePoll = false;
      }
      lastPoll = millis();
      pollConfig();
    }
  }
}

void uplinkBegin() {
  s_lock = xSemaphoreCreateMutex();
  loadConfig();
  {
    Lock l;
    s_st.queue = countLines();
    s_forceSend = s_st.queue > 0;  // pendências de antes do reinício
    s_st.configVersion = settings().cloudConfigVersion;
  }
  log_i("[%s] nuvem %s; fila com %u médias; configuração v%u", TAG,
        s_st.configured ? s_st.url.c_str() : "não configurada", s_st.queue, s_st.configVersion);
  xTaskCreatePinnedToCore(netTask, "cloud", 16384, nullptr, 1, nullptr, 0);
}

// Aplica no loop principal a configuração que a tarefa de rede recebeu.
void uplinkLoop() {
  String body;
  uint32_t version;
  {
    Lock l;
    if (s_pendingConfig.isEmpty()) return;
    body = s_pendingConfig;
    version = s_pendingVersion;
    s_pendingConfig = "";
  }
  JsonDocument doc;
  deserializeJson(doc, body);
  String err;
  if (settingsApplyCloud(doc["config"].as<JsonObjectConst>(), version, err)) {
    displayApplySettings();
    Lock l;
    s_st.configVersion = version;
    s_forcePoll = true;  // confirma a versão aplicada para o painel
    log_i("[%s] configuração v%u aplicada", TAG, version);
  } else {
    log_e("[%s] configuração v%u recusada: %s", TAG, version, err.c_str());
  }
}
