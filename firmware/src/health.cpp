#include "health.h"

#include <ArduinoOTA.h>
#include <Preferences.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <time.h>

#include "config.h"
#include "display.h"
#include "netmgr.h"
#include "settings.h"
#include "uplink.h"

static const char *TAG = "health";

static char s_reason[48] = "desconhecido";
static uint32_t s_boots = 0;
static bool s_otaBegun = false, s_otaActive = false;
static String s_otaPass;
static uint32_t s_offlineSince = 0;   // millis() desde quando está sem Wi-Fi (0 = conectado)
static uint32_t s_bootEpoch = 0;      // primeira hora válida depois do boot

static const char *resetName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "energia ligada";
    case ESP_RST_SW: return "reinício pedido";
    case ESP_RST_PANIC: return "travamento (exceção)";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "queda de tensão";
    case ESP_RST_EXT: return "botão de reset";
    case ESP_RST_DEEPSLEEP: return "volta do sono";
    default: return "desconhecido";
  }
}

void healthBegin() {
  Preferences p;
  p.begin("gweather", false);
  s_boots = p.getUInt("boots", 0) + 1;
  p.putUInt("boots", s_boots);
  String why = p.getString("why", "");
  if (why.length()) p.remove("why");
  p.end();
  esp_reset_reason_t r = esp_reset_reason();
  // Reinício pedido por nós mesmos: mostra o motivo guardado (autocura, API, console, OTA).
  if (r == ESP_RST_SW && why.length()) strlcpy(s_reason, why.c_str(), sizeof s_reason);
  else strlcpy(s_reason, resetName(r), sizeof s_reason);
  log_i("[%s] boot %u; último reinício: %s", TAG, s_boots, s_reason);

  // Watchdog do loop principal (a tarefa da nuvem se registra sozinha).
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
  esp_task_wdt_add(nullptr);
}

void healthRestart(const char *why) {
  Preferences p;
  p.begin("gweather", false);
  p.putString("why", why);
  p.end();
  log_w("[%s] reiniciando: %s", TAG, why);
  delay(200);
  ESP.restart();
}

static void beginOta() {
  ArduinoOTA.setHostname(settings().hostname.c_str());
  ArduinoOTA.setMdnsEnabled(false);  // o mDNS já é do netmgr (grain.local)
  s_otaPass = netStatus().apPass;
  ArduinoOTA.setPassword(s_otaPass.c_str());
  ArduinoOTA.onStart([]() {
    s_otaActive = true;
    log_i("[%s] atualização pelo Wi-Fi começou", TAG);
    displayOta(0, nullptr);
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    esp_task_wdt_reset();  // a gravação roda dentro do handle(): avisa o watchdog
    digitalWrite(PIN_LED, (millis() / 80) % 2);  // LED azul pisca rápido durante a gravação
    static int last = -1;
    int pct = total ? done * 100 / total : 0;
    if (pct != last) {
      last = pct;
      displayOta(pct, nullptr);
    }
  });
  ArduinoOTA.onEnd([]() {
    displayOta(100, "reiniciando...");
    Preferences p;
    p.begin("gweather", false);
    p.putString("why", "atualização pelo Wi-Fi");
    p.end();
  });
  ArduinoOTA.onError([](ota_error_t e) {
    s_otaActive = false;
    digitalWrite(PIN_LED, LOW);
    log_e("[%s] atualização falhou (%u)", TAG, (unsigned)e);
    displayOta(-1, e == OTA_AUTH_ERROR ? "senha incorreta" : "falhou");
  });
  ArduinoOTA.begin();
  s_otaBegun = true;
  log_i("[%s] atualização pelo Wi-Fi pronta (porta 3232)", TAG);
}

void healthLoop() {
  esp_task_wdt_reset();
  const NetStatus &n = netStatus();
  uint32_t ms = millis();

  // OTA: começa quando há Wi-Fi; acompanha a troca da senha da estação.
  if (n.connected && !n.portalActive) {
    if (!s_otaBegun) beginOta();
    else if (n.apPass != s_otaPass) {
      s_otaPass = n.apPass;
      ArduinoOTA.setPassword(s_otaPass.c_str());
    }
    ArduinoOTA.handle();
  }

  // Autocura: muito tempo sem Wi-Fi (fora da rede de configuração).
  if (n.connected || n.portalActive) s_offlineSince = 0;
  else if (!s_offlineSince) s_offlineSince = ms ? ms : 1;
  else if ((uint32_t)(ms - s_offlineSince) > NET_RESTART_AFTER_MS) healthRestart("sem Wi-Fi por 30 min");

  // Autocura: nuvem falhando há muito tempo com o Wi-Fi funcionando (ex.: TLS travado).
  // Confere a cada 10 s. A hora é lida DEPOIS do estado da nuvem: a tarefa da rede pode gravar
  // lastOkEpoch enquanto esperamos o lock, e um lastOkEpoch 1 s à frente de `now` fazia a
  // subtração sem sinal dar ~4 bilhões e reiniciar a estação sem motivo (bug do 0.14.0).
  static uint32_t lastCheck = 0;
  if (n.timeSynced && (uint32_t)(ms - lastCheck) >= 10000) {
    lastCheck = ms;
    const UplinkStatus u = uplinkStatus();
    uint32_t now = (uint32_t)time(nullptr);
    if (!s_bootEpoch) s_bootEpoch = now;
    uint32_t since = u.lastOkEpoch > s_bootEpoch ? u.lastOkEpoch : s_bootEpoch;
    if (u.configured && n.connected && now > since && now - since > CLOUD_RESTART_AFTER_MS / 1000)
      healthRestart("nuvem sem resposta por 6 h");
  }
}

const char *healthResetReason() { return s_reason; }
uint32_t healthBoots() { return s_boots; }
bool healthOtaActive() { return s_otaActive; }
