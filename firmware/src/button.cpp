#include "button.h"

#include "config.h"

static const char *TAG = "botao";

static bool s_raw = false, s_stable = false;  // true = apertado
static uint32_t s_rawSince = 0, s_pressedAt = 0;
static bool s_ignore = false;                 // apertado no boot: espera soltar
static const char *s_lastName = nullptr;
static uint32_t s_lastEpoch = 0;

void buttonBegin() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  s_raw = s_stable = digitalRead(PIN_BUTTON) == LOW;
  s_ignore = s_stable;
  s_rawSince = millis();
  if (s_ignore) log_w("[%s] apertado no boot: ignorado até soltar", TAG);
}

static BtnEvent finish(BtnEvent e, const char *name) {
  if (name) {
    s_lastName = name;
    s_lastEpoch = (uint32_t)time(nullptr);
    log_i("[%s] %s", TAG, name);
  }
  return e;
}

BtnEvent buttonPoll() {
  uint32_t now = millis();
  bool raw = digitalRead(PIN_BUTTON) == LOW;
  if (raw != s_raw) {
    s_raw = raw;
    s_rawSince = now;
  }
  if (s_raw == s_stable || (uint32_t)(now - s_rawSince) < BTN_DEBOUNCE_MS) return BTN_NONE;
  s_stable = s_raw;
  if (s_stable) {  // apertou
    s_pressedAt = now;
    return BTN_NONE;
  }
  // soltou
  if (s_ignore) {
    s_ignore = false;
    return BTN_NONE;
  }
  uint32_t held = now - s_pressedAt;
  if (held < BTN_TAP_MAX_MS) return finish(BTN_TAP, "toque");
  if (held < BTN_RAIN_MS) return finish(BTN_NONE, "cancelado");
  if (held < BTN_SCREEN_MS) return finish(BTN_RAIN, "chovendo (2 s)");
  if (held < BTN_CANCEL_MS) return finish(BTN_SCREEN, "tela (5 s)");
  return finish(BTN_NONE, "cancelado");
}

uint32_t buttonHeldMs() {
  if (!s_stable || s_ignore) return 0;
  uint32_t h = millis() - s_pressedAt;
  return h ? h : 1;
}

const char *buttonLastName() { return s_lastName; }
uint32_t buttonLastEpoch() { return s_lastEpoch; }
