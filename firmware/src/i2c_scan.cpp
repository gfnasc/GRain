#include "i2c_scan.h"

#include <Arduino.h>

#include "config.h"

static const char *TAG = "i2c";

static const char *knownName(uint8_t addr) {
  switch (addr) {
    case ADDR_AHT20: return "AHT20";
    case ADDR_BMP280: return "BMP280 (SDO em GND)";
    case ADDR_BMP280_ALT: return "BMP280 (SDO em 3V3)";
    case ADDR_OLED: return "OLED SSD1306";
    case 0x3D: return "OLED SSD1306 (endereço alternativo)";
    default: return "desconhecido";
  }
}

int i2cScan(TwoWire &bus) {
  int found = 0;
  Serial.println("\n--- Scanner I2C (SDA=D21, SCL=D22) ---");
  for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    bus.beginTransmission(addr);
    uint8_t err = bus.endTransmission();
    if (err == 0) {
      Serial.printf("  0x%02X  %s\n", addr, knownName(addr));
      found++;
    } else if (err == 4) {
      log_w("[%s] erro desconhecido em 0x%02X", TAG, addr);
    }
  }
  if (found == 0) {
    Serial.println("  Nenhum dispositivo respondeu. Confira 3V3, GND, SDA e SCL.");
  }
  Serial.printf("--- %d dispositivo(s) ---\n\n", found);
  return found;
}
