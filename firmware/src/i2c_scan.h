#pragma once

#include <Wire.h>

// Varre o barramento I2C e imprime os endereços que responderam,
// identificando os módulos esperados. Retorna quantos responderam.
int i2cScan(TwoWire &bus);
