#include "buzzer.h"

#include "config.h"
#include "netmgr.h"
#include "settings.h"

static const char *TAG = "buzzer";

// Sequências: durações em ms alternando ligado/desligado, começando ligado (0 = fim).
static const uint16_t P_BOOT[] = {30, 0};
static const uint16_t P_CLICK[] = {15, 0};
static const uint16_t P_ALERT[] = {50, 90, 50, 90, 50, 0};
static const uint16_t P_CONFIRM[] = {50, 90, 50, 0};
static const uint16_t *const PATTERNS[] = {P_BOOT, P_CLICK, P_ALERT, P_CONFIRM};

static const uint16_t *s_seq = nullptr;
static int s_step = 0;
static uint32_t s_stepAt = 0;
static uint32_t s_rev = 0;

static void out(bool on) {
  bool low = settings().buzzerActiveLow;
  digitalWrite(PIN_BUZZER, on != low ? HIGH : LOW);
}

void buzzerBegin() {
  pinMode(PIN_BUZZER, OUTPUT);
  out(false);
  s_rev = settingsRevision();
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

void buzzerPlay(BuzzPattern p, bool force) {
  if (!force) {
    const Settings &c = settings();
    if (!c.buzzerEnabled || quietNow()) return;
    if (p == BZ_CLICK && !c.buzzerClicks) return;
  }
  s_seq = PATTERNS[p];
  s_step = 0;
  s_stepAt = millis();
  out(true);
}

void buzzerStop() {
  s_seq = nullptr;
  out(false);
}

void buzzerLoop() {
  // Mudou o nível do buzzer na configuração: reaplica o "desligado" na hora.
  if (settingsRevision() != s_rev) {
    s_rev = settingsRevision();
    if (!s_seq) out(false);
  }
  if (!s_seq) return;
  uint32_t now = millis();
  if ((uint32_t)(now - s_stepAt) < s_seq[s_step]) return;
  s_stepAt = now;
  s_step++;
  if (!s_seq[s_step]) {
    buzzerStop();
    return;
  }
  out(s_step % 2 == 0);  // passos pares ligam, ímpares desligam
  (void)TAG;
}
