#include "sensors.h"

#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <Arduino.h>

#include "config.h"
#include "weather_math.h"

static const char *TAG = "sensors";

static TwoWire *s_bus = nullptr;
static Adafruit_AHTX0 s_aht;
static Adafruit_BMP280 *s_bmp = nullptr;
static bool s_ahtPresent = false;
static bool s_bmpPresent = false;
static uint8_t s_bmpAddr = 0;
static uint8_t s_bmpChipId = 0;
static float s_altitudeM = 0;
static int s_ahtFails = 0;
static int s_bmpFails = 0;

static bool beginAht() {
  s_ahtPresent = s_aht.begin(s_bus);
  if (s_ahtPresent) {
    log_i("[%s] AHT20 ok em 0x%02X", TAG, ADDR_AHT20);
  } else {
    log_e("[%s] AHT20 não respondeu", TAG);
  }
  return s_ahtPresent;
}

static bool beginBmp() {
  if (!s_bmp) s_bmp = new Adafruit_BMP280(s_bus);
  s_bmpPresent = false;
  const uint8_t addrs[] = {ADDR_BMP280, ADDR_BMP280_ALT};
  for (uint8_t addr : addrs) {
    // 0x58 = BMP280; 0x60 = BME280 (aceito, mas a umidade dele não é usada)
    if (s_bmp->begin(addr, BMP280_CHIPID) || s_bmp->begin(addr, 0x60)) {
      s_bmpPresent = true;
      s_bmpAddr = addr;
      s_bmpChipId = s_bmp->sensorID();
      break;
    }
  }
  if (!s_bmpPresent) {
    log_e("[%s] BMP280 não respondeu em 0x76 nem 0x77", TAG);
    return false;
  }
  // Recomendação da Bosch para estação meteorológica: modo forçado, sem filtro.
  s_bmp->setSampling(Adafruit_BMP280::MODE_FORCED, Adafruit_BMP280::SAMPLING_X2,
                     Adafruit_BMP280::SAMPLING_X16, Adafruit_BMP280::FILTER_OFF,
                     Adafruit_BMP280::STANDBY_MS_1);
  log_i("[%s] BMP280 ok em 0x%02X, chip id 0x%02X", TAG, s_bmpAddr, s_bmpChipId);
  return true;
}

void sensorsBegin(TwoWire &bus, float altitudeM) {
  s_bus = &bus;
  s_altitudeM = altitudeM;
  beginAht();
  beginBmp();
}

Reading sensorsRead() {
  Reading r;

  if (s_ahtPresent) {
    sensors_event_t hum, temp;
    if (s_aht.getEvent(&hum, &temp) && !isnan(temp.temperature) && !isnan(hum.relative_humidity)) {
      r.ahtOk = true;
      r.tempC = temp.temperature;
      r.humPct = hum.relative_humidity;
      s_ahtFails = 0;
    } else {
      s_ahtFails++;
    }
  }
  if (!r.ahtOk && (!s_ahtPresent || s_ahtFails >= SENSOR_REINIT_AFTER_FAILS)) {
    log_w("[%s] reinicializando AHT20", TAG);
    s_ahtFails = 0;
    beginAht();
  }

  if (s_bmpPresent && s_bmp->takeForcedMeasurement()) {
    float t = s_bmp->readTemperature();
    float p = s_bmp->readPressure() / 100.0f;
    if (!isnan(t) && !isnan(p) && p > 300.0f && p < 1100.0f) {
      r.bmpOk = true;
      r.tempBmpC = t;
      r.stationHpa = p;
      r.seaLevelHpa = seaLevelPressureHpa(p, s_altitudeM);
      s_bmpFails = 0;
    }
  }
  if (!r.bmpOk) {
    if (s_bmpPresent) s_bmpFails++;
    if (!s_bmpPresent || s_bmpFails >= SENSOR_REINIT_AFTER_FAILS) {
      log_w("[%s] reinicializando BMP280", TAG);
      s_bmpFails = 0;
      beginBmp();
    }
  }

  if (r.ahtOk) {
    r.dewPointC = dewPointC(r.tempC, r.humPct);
    r.heatIndexC = heatIndexC(r.tempC, r.humPct);
  }
  return r;
}

bool sensorsAhtPresent() { return s_ahtPresent; }
bool sensorsBmpPresent() { return s_bmpPresent; }
uint8_t sensorsBmpAddress() { return s_bmpAddr; }
uint8_t sensorsBmpChipId() { return s_bmpChipId; }
