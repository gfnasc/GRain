#pragma once

#include <Wire.h>

struct Reading {
  bool ahtOk = false;
  bool bmpOk = false;
  float tempC = NAN;        // AHT20 (fonte principal)
  float humPct = NAN;       // AHT20
  float tempBmpC = NAN;     // BMP280 (conferência)
  float stationHpa = NAN;   // BMP280, pressão no local
  float seaLevelHpa = NAN;  // corrigida para o nível do mar
  float dewPointC = NAN;
  float heatIndexC = NAN;
};

// Inicializa AHT20 e BMP280. Pode ser chamada de novo para reinicializar.
void sensorsBegin(TwoWire &bus, float altitudeM);

// Atualiza a altitude usada na correção da pressão.
void sensorsSetAltitude(float altitudeM);

// Faz uma leitura completa e calcula os valores derivados.
Reading sensorsRead();

// Última leitura feita e o millis() em que foi feita (0 = nenhuma ainda).
const Reading &sensorsLast();
uint32_t sensorsLastMs();
float sensorsAltitude();

// Mínima e máxima de hoje (zera à meia-noite local; só conta com a hora sincronizada).
struct TodayStats {
  bool valid = false;
  float tMin = NAN, tMax = NAN, hMin = NAN, hMax = NAN;
  uint32_t tMinTs = 0, tMaxTs = 0;  // epoch UTC
};
const TodayStats &sensorsToday();

bool sensorsAhtPresent();
bool sensorsBmpPresent();
uint8_t sensorsBmpAddress();
uint8_t sensorsBmpChipId();
