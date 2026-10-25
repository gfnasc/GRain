#include <Arduino.h>
#include <Wire.h>

#include "button.h"
#include "buzzer.h"
#include "config.h"
#include "console.h"
#include "display.h"
#include "health.h"
#include "i2c_scan.h"
#include "led.h"
#include "netmgr.h"
#include "sensors.h"
#include "settings.h"
#include "uplink.h"
#include "webapi.h"

static const char *TAG = "main";

// Uma linha por leitura, para não poluir o console serial.
static void printReading(const Reading &r) {
  char t[12];
  if (!netLocalTime(t, sizeof t, "%H:%M:%S")) snprintf(t, sizeof t, "%lus", (unsigned long)(millis() / 1000));
  Serial.printf("[%s] ", t);
  if (r.ahtOk)
    Serial.printf("%.2f °C  %.1f %%  orvalho %.1f °C  sensação %.1f °C", r.tempC, r.humPct, r.dewPointC, r.heatIndexC);
  else
    Serial.print("AHT20 sem leitura");
  if (r.bmpOk) {
    Serial.printf("  |  %.2f hPa (local %.2f)  BMP %.2f °C", r.seaLevelHpa, r.stationHpa, r.tempBmpC);
    if (r.ahtOk && fabsf(r.tempBmpC - r.tempC) > TEMP_MISMATCH_WARN_C) Serial.print("  <-- diferença AHT/BMP alta");
  } else {
    Serial.print("  |  BMP280 sem leitura");
  }
  Serial.println();
}

// Gestos do botão (Fase 19).
static void handleButton() {
  switch (buttonPoll()) {
    case BTN_TAP:
      displayTap();
      buzzerPlay(BZ_CLICK);
      break;
    case BTN_RAIN: {
      bool cloud = uplinkStatus().configured;
      if (cloud) uplinkReportRain(false);
      displayRainReported(cloud);
      buzzerPlay(BZ_CONFIRM);
      break;
    }
    case BTN_SCREEN:
      displayToggleScreen();
      buzzerPlay(BZ_CLICK);
      break;
    default:
      break;
  }
}

void setup() {
  // Configuração primeiro: o buzzer precisa saber em que nível fica calado.
  settingsBegin();
  buzzerBegin();
  Serial.begin(115200);
  delay(500);
  ledBegin();
  buttonBegin();
  log_i("[%s] boot: GRain %s", TAG, FW_VERSION);
  healthBegin();

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
  // Display primeiro, para a tela de abertura aparecer durante a inicialização.
  bool oledOk = displayBegin();
  i2cScan(Wire);

  sensorsBegin(Wire, settingsAltitudeM());

  Serial.printf("AHT20 : %s\n", sensorsAhtPresent() ? "ok (0x38)" : "NÃO ENCONTRADO");
  if (sensorsBmpPresent()) {
    uint8_t id = sensorsBmpChipId();
    Serial.printf("BMP280: ok (0x%02X), chip id 0x%02X = %s\n", sensorsBmpAddress(), id,
                  id == 0x58 ? "BMP280" : (id == 0x60 ? "BME280" : "desconhecido"));
  } else {
    Serial.println("BMP280: NÃO ENCONTRADO");
  }
  Serial.printf("OLED  : %s\n", oledOk ? "ok (0x3C)" : "NÃO ENCONTRADO");
  Serial.printf("Altitude do sensor: %.1f m\n", settingsAltitudeM());

  netBegin();
  uplinkBegin();
  Serial.println("Digite \"ajuda\" para ver os comandos.\n");
  buzzerPlay(BZ_BOOT);  // bipe curtíssimo: terminou de ligar
}

void loop() {
  healthLoop();
  if (healthOtaActive()) return;  // durante a atualização pelo Wi-Fi, só ela roda
  netLoop();
  webapiLoop();
  uplinkLoop();
  consoleLoop();
  handleButton();
  buzzerLoop();
  ledLoop();

  static uint32_t last = 0;
  uint32_t now = millis();
  if (last == 0 || (uint32_t)(now - last) >= SENSOR_INTERVAL_MS) {
    last = now ? now : 1;
    Reading r = sensorsRead();
    printReading(r);
    uplinkAddReading(r);
    displayUpdate(r);
    ledPulse();
  }
  displayLoop();
}
