#include <Arduino.h>
#include <Wire.h>

#include "config.h"
#include "display.h"
#include "i2c_scan.h"
#include "sensors.h"
#include "weather_math.h"

static const char *TAG = "main";
static float s_altitudeM = 0;

static void printReading(const Reading &r) {
  Serial.println("----------------------------------------------");
  if (r.ahtOk) {
    Serial.printf("Temperatura (AHT20) : %6.2f °C\n", r.tempC);
    Serial.printf("Umidade     (AHT20) : %6.1f %%\n", r.humPct);
    Serial.printf("Ponto de orvalho    : %6.1f °C\n", r.dewPointC);
    Serial.printf("Sensação térmica    : %6.1f °C\n", r.heatIndexC);
  } else {
    Serial.println("AHT20               : sem leitura");
  }
  if (r.bmpOk) {
    Serial.printf("Temperatura (BMP280): %6.2f °C\n", r.tempBmpC);
    Serial.printf("Pressão no local    : %7.2f hPa\n", r.stationHpa);
    Serial.printf("Pressão nível do mar: %7.2f hPa  (altitude %.1f m)\n", r.seaLevelHpa, s_altitudeM);
  } else {
    Serial.println("BMP280              : sem leitura");
  }
  if (r.ahtOk && r.bmpOk) {
    float diff = r.tempBmpC - r.tempC;
    Serial.printf("Diferença BMP-AHT   : %+6.2f °C%s\n", diff,
                  fabsf(diff) > TEMP_MISMATCH_WARN_C ? "  <-- acima do esperado" : "");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(PIN_LED, OUTPUT);
  log_i("[%s] boot: GWeather", TAG);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
  i2cScan(Wire);

  s_altitudeM = sensorAltitudeM(SITE_GROUND_ALT_M, SITE_FLOOR, SITE_FLOOR_HEIGHT_M, SITE_SENSOR_HEIGHT_M);
  sensorsBegin(Wire, s_altitudeM);

  Serial.printf("AHT20 : %s\n", sensorsAhtPresent() ? "ok (0x38)" : "NÃO ENCONTRADO");
  if (sensorsBmpPresent()) {
    uint8_t id = sensorsBmpChipId();
    Serial.printf("BMP280: ok (0x%02X), chip id 0x%02X = %s\n", sensorsBmpAddress(), id,
                  id == 0x58 ? "BMP280" : (id == 0x60 ? "BME280" : "desconhecido"));
  } else {
    Serial.println("BMP280: NÃO ENCONTRADO");
  }
  Serial.printf("Altitude do sensor: %.1f m\n", s_altitudeM);
  Serial.printf("OLED  : %s\n", displayBegin() ? "ok (0x3C)" : "NÃO ENCONTRADO");
}

void loop() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (last == 0 || (uint32_t)(now - last) >= SENSOR_INTERVAL_MS) {
    last = now ? now : 1;
    digitalWrite(PIN_LED, HIGH);
    Reading r = sensorsRead();
    printReading(r);
    displayUpdate(r);
    digitalWrite(PIN_LED, LOW);
  }
  displayLoop();
}
