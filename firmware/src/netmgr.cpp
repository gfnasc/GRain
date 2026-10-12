#include "netmgr.h"

#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>

#include "config.h"
#include "settings.h"

static const char *TAG = "net";

static WiFiManager s_wm;
static NetStatus s_st;
static bool s_mdnsUp = false;
static uint32_t s_lastCheck = 0;
static uint32_t s_disconnectedSince = 0;

// Senha da rede de configuração: aleatória, gerada no primeiro boot e
// guardada na NVS. Aparece só no display, então só quem vê a estação
// consegue configurá-la. (Não derivar do MAC: ele é visível no ar.)
static String portalPassword() {
  Preferences prefs;
  prefs.begin("gweather", false);
  String pass = prefs.getString("portal_pass", "");
  if (pass.length() != 8) {
    static const char alphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";  // sem 0/o, 1/l/i
    pass = "";
    for (int i = 0; i < 8; i++) pass += alphabet[esp_random() % (sizeof alphabet - 1)];
    prefs.putString("portal_pass", pass);
  }
  prefs.end();
  return pass;
}

static void startMdns() {
  if (s_mdnsUp) return;
  const String &host = settings().hostname;
  if (MDNS.begin(host.c_str())) {
    s_mdnsUp = true;
    log_i("[%s] mDNS: %s.local", TAG, host.c_str());
  } else {
    log_e("[%s] falha ao iniciar mDNS", TAG);
  }
}

static void startTime() {
  const Settings &c = settings();
  configTzTime(c.tz.c_str(), c.ntp1.c_str(), c.ntp2.c_str());
  log_i("[%s] NTP: %s, %s (fuso %s)", TAG, c.ntp1.c_str(), c.ntp2.c_str(), c.tz.c_str());
}

static void onConnected() {
  s_st.connected = true;
  s_st.ssid = WiFi.SSID();
  s_st.ip = WiFi.localIP().toString();
  s_disconnectedSince = 0;
  log_i("[%s] conectado em \"%s\", IP %s", TAG, s_st.ssid.c_str(), s_st.ip.c_str());
  startMdns();
  startTime();
}

void netBegin() {
  WiFi.mode(WIFI_STA);  // com o rádio ligado, esp_random() usa entropia de hardware
  s_st.apName = PORTAL_AP_NAME;
  s_st.apPass = portalPassword();

  WiFi.setHostname(settings().hostname.c_str());
  WiFi.setAutoReconnect(true);

  s_wm.setConfigPortalBlocking(false);
  s_wm.setConnectTimeout(20);
  // Sem Wi-Fi salvo, a rede de configuração fica aberta até ser usada.
  // Com Wi-Fi salvo (ex.: roteador fora do ar no boot), fecha sozinha depois de um tempo.
  s_wm.setConfigPortalTimeout(s_wm.getWiFiIsSaved() ? PORTAL_TIMEOUT_S : 0);
  s_wm.setTitle("GWeather");
  static std::vector<const char *> menu = {"wifi", "info", "exit"};
  s_wm.setMenu(menu);
  s_wm.setClass("invert");
  s_wm.setShowInfoUpdate(false);
  s_wm.setShowInfoErase(false);
  s_wm.setHostname(settings().hostname.c_str());

  if (s_wm.autoConnect(s_st.apName.c_str(), s_st.apPass.c_str())) {
    onConnected();
  } else {
    s_st.portalActive = true;
    log_i("[%s] rede de configuração aberta: %s", TAG, s_st.apName.c_str());
  }
}

void netLoop() {
  s_wm.process();
  s_st.portalActive = s_wm.getConfigPortalActive() || s_wm.getWebPortalActive();

  uint32_t now = millis();
  if ((uint32_t)(now - s_lastCheck) < 1000) return;
  s_lastCheck = now;

  bool up = WiFi.status() == WL_CONNECTED;
  if (up && !s_st.connected) onConnected();
  if (!up && s_st.connected) {
    s_st.connected = false;
    log_w("[%s] Wi-Fi caiu; tentando reconectar", TAG);
  }
  if (up) {
    s_st.rssi = WiFi.RSSI();
  } else if (!s_st.portalActive) {
    if (s_disconnectedSince == 0) s_disconnectedSince = now;
    if ((uint32_t)(now - s_disconnectedSince) > WIFI_RETRY_MS) {
      s_disconnectedSince = now;
      log_w("[%s] ainda sem Wi-Fi; nova tentativa", TAG);
      WiFi.reconnect();
    }
  }

  if (!s_st.timeSynced && time(nullptr) > 1700000000) {
    s_st.timeSynced = true;
    char buf[32];
    netLocalTime(buf, sizeof buf, "%d/%m/%Y %H:%M:%S");
    log_i("[%s] hora sincronizada: %s", TAG, buf);
  }
}

const NetStatus &netStatus() { return s_st; }

bool netLocalTime(char *out, size_t n, const char *fmt) {
  if (!s_st.timeSynced) return false;
  time_t t = time(nullptr);
  struct tm lt;
  localtime_r(&t, &lt);
  strftime(out, n, fmt, &lt);
  return true;
}

void netForgetWifi() {
  log_w("[%s] apagando Wi-Fi salvo e reiniciando", TAG);
  s_wm.resetSettings();
  delay(300);
  ESP.restart();
}

void netOpenPortal() {
  if (s_st.portalActive) return;
  log_i("[%s] abrindo rede de configuração: %s", TAG, s_st.apName.c_str());
  s_wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
  s_wm.startConfigPortal(s_st.apName.c_str(), s_st.apPass.c_str());
  s_st.portalActive = true;
}
