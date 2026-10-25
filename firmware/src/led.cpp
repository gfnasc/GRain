#include "led.h"

#include <Arduino.h>

#include "button.h"
#include "buzzer.h"
#include "config.h"
#include "display.h"
#include "netmgr.h"
#include "settings.h"
#include "uplink.h"

static uint32_t s_pulseUntil = 0;
static uint32_t s_confirmUntil = 0, s_confirmAt = 0;
static uint32_t s_lastReport = 0;
static bool s_pending = false;
static uint32_t s_lastEval = 0;
static bool s_problem = false, s_alert = false;
static int s_on = -1;
static bool s_first = true;

// Não consulta a nuvem aqui: ledBegin() roda antes de uplinkBegin() criar o lock (o 0.15.0
// fazia isso e a placa reiniciava em loop no boot).
void ledBegin() {
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
}

void ledPulse() {
  s_pulseUntil = millis() + 40;
  if (!s_pulseUntil) s_pulseUntil = 1;
}

static void set(bool on) {
  if ((int)on == s_on) return;
  s_on = on;
  digitalWrite(PIN_LED, on ? HIGH : LOW);
}

// Padrão de piscadas: n piscadas de 80 ms (com 120 ms entre elas) a cada `period` ms.
static bool blinks(uint32_t now, int n, uint32_t period) {
  uint32_t t = now % period;
  for (int i = 0; i < n; i++)
    if (t >= (uint32_t)i * 200 && t < (uint32_t)i * 200 + 80) return true;
  return false;
}

void ledLoop() {
  uint32_t now = millis();
  // Estado da nuvem e dos avisos: a cada 50 ms (uplinkStatus copia a estrutura).
  if ((uint32_t)(now - s_lastEval) >= 50) {
    s_lastEval = now;
    const UplinkStatus &u = uplinkStatus();
    s_pending = u.reportPending || u.noRainPending;
    if (s_first) {
      s_first = false;
      s_lastReport = u.reportEpoch;
    }
    if (u.reportEpoch != s_lastReport) {
      if (u.reportEpoch) {
        s_confirmAt = now;
        s_confirmUntil = now + 450;
      }
      s_lastReport = u.reportEpoch;
    }
    s_problem = displayProblem();
    s_alert = displayRainAlert(nullptr, nullptr) && !displayAlertAcked();
  }

  const Settings &c = settings();
  uint8_t mode = quietNow() ? c.ledQuietMode : c.ledMode;  // 0 normal, 1 só avisos, 2 desligado
  if (mode == 2) return set(false);

  if (netStatus().portalActive) return set(true);

  uint32_t held = buttonHeldMs();
  if (held) {
    if (held < BTN_RAIN_MS) return set(true);
    if (held < BTN_SCREEN_MS) return set((now / 100) % 2 == 0);
    if (held < BTN_CANCEL_MS) return set(true);
    return set(false);
  }
  if (s_confirmUntil && (int32_t)(now - s_confirmUntil) < 0) {
    uint32_t t = now - s_confirmAt;  // 2 piscadas de 150 ms
    return set(t < 150 || (t >= 300 && t < 450));
  }
  if (s_pending) return set((now / 500) % 2 == 0);
  if (s_alert) return set(blinks(now, 2, 2000));
  if (s_problem) return set(blinks(now, 3, 5000));
  if (mode == 0 && s_pulseUntil && (int32_t)(now - s_pulseUntil) < 0) return set(true);
  set(false);
}
