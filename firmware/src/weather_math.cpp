#include "weather_math.h"

#include <math.h>

float sensorAltitudeM(float groundAltM, int floor, float floorHeightM, float sensorHeightM) {
  return groundAltM + floor * floorHeightM + sensorHeightM;
}

float seaLevelPressureHpa(float stationHpa, float altitudeM) {
  return stationHpa / powf(1.0f - altitudeM / 44330.0f, 5.255f);
}

float dewPointC(float tempC, float humPct) {
  const float a = 17.62f, b = 243.12f;
  if (humPct <= 0.0f) return NAN;
  float g = logf(humPct / 100.0f) + a * tempC / (b + tempC);
  return b * g / (a - g);
}

float heatIndexC(float tempC, float humPct) {
  // Algoritmo do NWS/NOAA, calculado em °F.
  float t = tempC * 9.0f / 5.0f + 32.0f;
  float rh = humPct;
  float hi = 0.5f * (t + 61.0f + (t - 68.0f) * 1.2f + rh * 0.094f);

  if ((hi + t) / 2.0f >= 80.0f) {
    hi = -42.379f + 2.04901523f * t + 10.14333127f * rh - 0.22475541f * t * rh -
         0.00683783f * t * t - 0.05481717f * rh * rh + 0.00122874f * t * t * rh +
         0.00085282f * t * rh * rh - 0.00000199f * t * t * rh * rh;
    if (rh < 13.0f && t >= 80.0f && t <= 112.0f) {
      hi -= ((13.0f - rh) / 4.0f) * sqrtf((17.0f - fabsf(t - 95.0f)) / 17.0f);
    } else if (rh > 85.0f && t >= 80.0f && t <= 87.0f) {
      hi += ((rh - 85.0f) / 10.0f) * ((87.0f - t) / 5.0f);
    }
  }
  return (hi - 32.0f) * 5.0f / 9.0f;
}
