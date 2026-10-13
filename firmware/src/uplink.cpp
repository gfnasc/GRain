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
static String s_configCopy;        // JSON da cópia da configuração (montado no loop principal)
static float s_pendingOffset = NAN; // desvio da pressão sugerido pela nuvem, a aplicar no loop principal

struct Lock {
  Lock() { xSemaphoreTake(s_lock, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(s_lock); }
};

// Acumulador do intervalo atual de 10 min (só no loop principal).
struct Bucket {
  uint32_t start = 0;  // epoch UTC do início do intervalo
  uint32_t nAht = 0, nBmp = 0;
  double temp = 0, hum = 0, dew = 0, pres = 0, presRaw = 0;
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
    if (b.nBmp >= minSamples) {
      doc["pres_slp"] = roundf(b.pres / b.nBmp * 100) / 100;
      doc["pres_raw"] = roundf(b.presRaw / b.nBmp * 100) / 100;
    }
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
    s_bucket.presRaw += r.seaLevelRawHpa;
  }
}

// --- configuração (NVS) ---

static void loadConfig() {
  Preferences p;
  p.begin("gweather", true);  // namespace da NVS mantido desde o GWeather (URL e token)
  String url = p.getString("cloud_url", "");
  String tok = p.getString("cloud_token", "");
  p.end();
  Lock l;
  s_st.url = url;
  s_token = tok;
  s_st.configured = url.length() && tok.length();
  s_backoffMs = 0;
  s_st.configPending = true;  // endereço ou token novos: manda a cópia de novo
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
  s_st.configPending = true;
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
  http.setConnectTimeout(10000);
  http.setReuse(false);  // uma conexão por requisição: nada fica pendurado entre consultas
  if (!http.begin(client, url)) return -1;
  http.addHeader("Authorization", "Bearer " + token);
  int code;
  if (strcmp(method, "POST") == 0) {
    http.addHeader("Content-Type", "application/json");
    code = http.POST(body);
  } else {
    code = http.GET();
  }
  // 204 não tem corpo: ler aqui travaria esperando dados que nunca chegam.
  if (code == 204) resp = "";
  else resp = code > 0 ? http.getString() : http.errorToString(code);
  http.end();
  log_d("[%s] %s %s -> %d", TAG, method, url.c_str(), code);
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
    JsonDocument doc;
    JsonArray arr = doc["readings"].to<JsonArray>();
    File f = LittleFS.open(QUEUE, "r");
    if (f) {
      while (f.available() && n < UPLINK_BATCH_MAX) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (!line.length()) continue;
        JsonDocument item;
        n++;  // linha corrompida também conta, para ser descartada junto
        if (!deserializeJson(item, line)) arr.add(item);
      }
      f.close();
    } else {
      s_st.queue = 0;
    }
    if (n == 0 && !s_st.configPending) return;
    if (s_configCopy.length()) doc["config"] = serialized(s_configCopy);
    JsonObject st = doc["status"].to<JsonObject>();
    st["fw"] = FW_VERSION;
    st["rssi"] = WiFi.RSSI();
    st["uptime_s"] = millis() / 1000;
    st["heap"] = ESP.getFreeHeap();
    st["queue"] = s_st.queue > n ? s_st.queue - n : 0;  // pendências depois deste lote
    serializeJson(doc, body);
  }

  String resp;
  int code = https("POST", url, token, body, resp);

  Lock l;
  s_st.lastHttp = code;
  if (code == 200) {
    if (n) dropLines(n);
    s_st.lastOkEpoch = (uint32_t)time(nullptr);
    if (body.indexOf("\"config\":") >= 0) {
      s_st.configSentEpoch = s_st.lastOkEpoch;
      s_st.configPending = false;
    }
    s_st.lastError = "";
    s_backoffMs = 0;
    s_nextAttemptMs = 0;
    log_i("[%s] lote de %u enviado: %s (fila %u)", TAG, n, resp.c_str(), s_st.queue);
    JsonDocument r;
    if (!deserializeJson(r, resp) && r["pres_offset"].is<float>()) s_pendingOffset = r["pres_offset"];
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

static void netTask(void *) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(500));
    const NetStatus &n = netStatus();
    bool configured, sendDue;
    {
      Lock l;
      configured = s_st.configured;
      bool backoffOk = !s_backoffMs || (int32_t)(millis() - s_nextAttemptMs) >= 0;
      bool work = s_st.queue > 0 || s_st.configPending;
      sendDue = work && backoffOk && (s_forceSend || s_st.configPending || s_backoffMs);
    }
    if (!configured || !n.connected || !n.timeSynced) continue;  // TLS precisa da hora certa
    if (sendDue) {
      {
        Lock l;
        s_forceSend = false;
      }
      sendBatch();
    }
  }
}

void uplinkBegin() {
  s_lock = xSemaphoreCreateMutex();
  loadConfig();
  uplinkLoop();  // monta a primeira cópia da configuração
  {
    Lock l;
    s_st.queue = countLines();
    s_forceSend = s_st.queue > 0;  // pendências de antes do reinício
  }
  log_i("[%s] nuvem %s; fila com %u médias", TAG, s_st.configured ? s_st.url.c_str() : "não configurada", s_st.queue);
  xTaskCreatePinnedToCore(netTask, "cloud", 16384, nullptr, 1, nullptr, 0);
}

// Remonta a cópia da configuração quando ela muda (só no loop principal,
// que é quem altera as configurações) e marca para enviar.
void uplinkLoop() {
  // Desvio da pressão vindo da nuvem: grava só se a calibração automática estiver ligada
  // e o valor mudou (evita escrever no flash a cada envio).
  float offset;
  {
    Lock l;
    offset = s_pendingOffset;
    s_pendingOffset = NAN;
  }
  if (!isnan(offset) && settings().presAuto && fabsf(offset - settings().presAutoOffset) >= 0.05f) {
    String err;
    if (settingsSet("calibration.pres_auto_offset", String(offset, 2), err))
      log_i("[%s] pressão calibrada pelo aeroporto: %+.2f hPa", TAG, offset);
  }

  static uint32_t seen = 0;
  uint32_t rev = settingsRevision();
  if (rev == seen) return;
  seen = rev;
  JsonDocument doc;
  settingsCloudCopy(doc);
  String json;
  serializeJson(doc, json);
  Lock l;
  s_configCopy = json;
  s_st.configPending = true;
}
