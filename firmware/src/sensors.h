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

// Faz uma leitura completa e calcula os valores derivados.
Reading sensorsRead();

bool sensorsAhtPresent();
bool sensorsBmpPresent();
uint8_t sensorsBmpAddress();
uint8_t sensorsBmpChipId();
