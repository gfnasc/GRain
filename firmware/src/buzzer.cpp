#include "buzzer.h"

#include <esp_timer.h>

#include "config.h"
#include "netmgr.h"
#include "settings.h"

// Sequências: durações em ms alternando ligado/desligado, começando ligado (0 = fim).
static const uint16_t P_BOOT[] = {30, 0};
static const uint16_t P_CLICK[] = {25, 0};
static const uint16_t P_ALERT[] = {50, 90, 50, 90, 50, 0};
static const uint16_t P_CONFIRM[] = {50, 90, 50, 0};
static const uint16_t *const PATTERNS[] = {P_BOOT, P_CLICK, P_ALERT, P_CONFIRM};

// O ritmo é dado por um temporizador do ESP32 (esp_timer), não pelo loop(): leitura de sensor
// (~80 ms), redesenho do OLED ou uma requisição HTTP no meio de uma sequência deixavam um bipe
// mais longo que os outros (0.15.3).
static esp_timer_handle_t s_timer = nullptr;
static const uint16_t *volatile s_seq = nullptr;
static volatile int s_step = 0;
static volatile bool s_low = false;  // nível que liga (cópia de settings, lida no callback)
static uint32_t s_rev = 0;
static bool s_bootPending = false;
// Repetição do alerta de chuva.
static const uint32_t ALERT_REPEAT_MS = 2000, ALERT_TOTAL_MS = 30000;
static uint32_t s_alertUntil = 0, s_alertNext = 0;
static bool s_alertForce = false;

static inline void out(bool on) { digitalWrite(PIN_BUZZER, on != s_low ? HIGH : LOW); }

static void onTimer(void *) {
  const uint16_t *seq = s_seq;
  if (!seq) return out(false);
  int step = s_step + 1;
  if (!seq[step]) {
    s_seq = nullptr;
    return out(false);
  }
  s_step = step;
  out(step % 2 == 0);  // passos pares ligam, ímpares desligam
  esp_timer_start_once(s_timer, (uint64_t)seq[step] * 1000);
}

void buzzerBegin() {
  s_low = settings().buzzerActiveLow;
  pinMode(PIN_BUZZER, OUTPUT);
  out(false);
  s_rev = settingsRevision();
  esp_timer_create_args_t args = {};
  args.callback = onTimer;
  args.name = "buzzer";
  esp_timer_create(&args, &s_timer);
}

bool quietNow() {
  const Settings &c = settings();
  if (!c.quietEnabled || !netStatus().timeSynced) return false;
  time_t now = time(nullptr);
  struct tm lt;
  localtime_r(&now, &lt);
  int m = lt.tm_hour * 60 + lt.tm_min;
  return c.quietFrom > c.quietTo ? (m >= c.quietFrom || m < c.quietTo) : (m >= c.quietFrom && m < c.quietTo);
}

static void start(BuzzPattern p) {
  if (!s_timer) return;
  esp_timer_stop(s_timer);
  s_seq = PATTERNS[p];
  s_step = 0;
  out(true);
  esp_timer_start_once(s_timer, (uint64_t)PATTERNS[p][0] * 1000);
}

// Sons de resposta a um gesto (clique, confirmação) tocam mesmo no horário silencioso: são a
// resposta a algo que a pessoa acabou de fazer. O silencioso cala os automáticos (alerta, boot).
// O bipe de boot espera a hora sincronizar (até 20 s) para saber se é horário silencioso.
void buzzerPlay(BuzzPattern p, bool force) {
  if (p == BZ_BOOT && !force) {
    s_bootPending = true;
    return;
  }
  if (!force) {
    const Settings &c = settings();
    if (!c.buzzerEnabled) return;
    if (p == BZ_CLICK && !c.buzzerClicks) return;
    if (p == BZ_ALERT && quietNow()) return;
  }
  start(p);
}

void buzzerAlertStart(bool force) {
  if (!force && (!settings().buzzerEnabled || quietNow())) return;
  uint32_t now = millis();
  s_alertForce = force;
  s_alertUntil = now + ALERT_TOTAL_MS;
  if (!s_alertUntil) s_alertUntil = 1;
  s_alertNext = now + ALERT_REPEAT_MS;
  start(BZ_ALERT);
}

void buzzerStop() {
  s_alertUntil = 0;
  if (s_timer) esp_timer_stop(s_timer);
  s_seq = nullptr;
  out(false);
}

void buzzerLoop() {
  // Mudou o nível do buzzer na configuração: reaplica o "desligado" na hora.
  if (settingsRevision() != s_rev) {
    s_rev = settingsRevision();
    s_low = settings().buzzerActiveLow;
    if (!s_seq) out(false);
  }
  if (s_alertUntil) {
    uint32_t now = millis();
    if ((int32_t)(now - s_alertUntil) >= 0) {
      s_alertUntil = 0;
    } else if ((int32_t)(now - s_alertNext) >= 0) {
      s_alertNext += ALERT_REPEAT_MS;
      if (s_alertForce || (settings().buzzerEnabled && !quietNow())) start(BZ_ALERT);
    }
  }
  if (s_bootPending && (netStatus().timeSynced || millis() > 20000)) {
    s_bootPending = false;
    if (settings().buzzerEnabled && !quietNow()) start(BZ_BOOT);
  }
}
