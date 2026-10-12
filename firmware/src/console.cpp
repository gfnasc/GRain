#include "console.h"

#include <Arduino.h>
#include <LittleFS.h>

#include "display.h"
#include "netmgr.h"
#include "sensors.h"
#include "settings.h"

static String s_line;

static void printHelp() {
  Serial.println(
      "\nComandos:\n"
      "  ajuda                 esta lista\n"
      "  status                rede, hora, memória e sensores\n"
      "  config                mostra a configuração (/config.json)\n"
      "  set <chave> <valor>   altera e salva. Chaves:\n"
      "                          hostname, time.tz, time.ntp1, time.ntp2,\n"
      "                          site.ground_alt_m, site.floor, site.floor_height_m,\n"
      "                          site.sensor_height_m, display.page_s, display.contrast\n"
      "  config padrao         volta aos valores iniciais (site.h)\n"
      "  wifi portal           abre a rede de configuração sem esquecer o Wi-Fi atual\n"
      "  wifi esquecer         apaga o Wi-Fi salvo e reinicia\n"
      "  reiniciar             reinicia a placa\n");
}

static void printStatus() {
  const NetStatus &n = netStatus();
  char t[32];
  Serial.println();
  if (n.connected)
    Serial.printf("Wi-Fi   : conectado em \"%s\", IP %s, sinal %d dBm\n", n.ssid.c_str(), n.ip.c_str(), n.rssi);
  else
    Serial.println("Wi-Fi   : desconectado");
  if (n.portalActive)
    Serial.printf("Portal  : aberto (rede \"%s\", senha %s, http://192.168.4.1)\n", n.apName.c_str(), n.apPass.c_str());
  Serial.printf("Endereço: http://%s.local  (API: /api/v1)\n", settings().hostname.c_str());
  Serial.printf("Senha   : %s  (rede de configuração e API, usuário \"admin\")\n", n.apPass.c_str());
  Serial.printf("Hora    : %s\n", netLocalTime(t, sizeof t, "%d/%m/%Y %H:%M:%S") ? t : "aguardando sincronização");
  Serial.printf("Altitude: %.1f m\n", settingsAltitudeM());
  Serial.printf("Sensores: AHT20 %s, BMP280 %s\n", sensorsAhtPresent() ? "ok" : "ERRO", sensorsBmpPresent() ? "ok" : "ERRO");
  Serial.printf("Memória : %u bytes livres; LittleFS %u/%u bytes\n", ESP.getFreeHeap(),
                (unsigned)LittleFS.usedBytes(), (unsigned)LittleFS.totalBytes());
  Serial.printf("Ligado  : %lu s\n", (unsigned long)(millis() / 1000));
}

static void handle(String line) {
  line.trim();
  if (line.isEmpty()) return;
  int sp = line.indexOf(' ');
  String cmd = sp < 0 ? line : line.substring(0, sp);
  String rest = sp < 0 ? "" : line.substring(sp + 1);
  rest.trim();

  if (cmd == "ajuda" || cmd == "help" || cmd == "?") {
    printHelp();
  } else if (cmd == "status") {
    printStatus();
  } else if (cmd == "config" && rest.isEmpty()) {
    settingsPrint();
  } else if (cmd == "config" && rest == "padrao") {
    Serial.println(settingsReset() ? "Configuração restaurada. Reinicie para aplicar rede e hora."
                                   : "Erro ao gravar a configuração.");
    sensorsSetAltitude(settingsAltitudeM());
    displayApplySettings();
  } else if (cmd == "set") {
    int sp2 = rest.indexOf(' ');
    if (sp2 < 0) {
      Serial.println("Uso: set <chave> <valor>");
      return;
    }
    String key = rest.substring(0, sp2), value = rest.substring(sp2 + 1);
    value.trim();
    String err;
    if (!settingsSet(key, value, err)) {
      Serial.printf("Não alterado: %s (%s)\n", key.c_str(), err.c_str());
      return;
    }
    Serial.printf("Salvo: %s = %s\n", key.c_str(), value.c_str());
    if (key.startsWith("site.")) sensorsSetAltitude(settingsAltitudeM());
    if (key.startsWith("display.")) displayApplySettings();
    if (key == "hostname" || key.startsWith("time.")) Serial.println("Reinicie para aplicar (comando: reiniciar).");
  } else if (cmd == "wifi" && rest == "portal") {
    netOpenPortal();
    Serial.println("Rede de configuração aberta. Veja nome e senha no display.");
  } else if (cmd == "wifi" && rest == "esquecer") {
    netForgetWifi();
  } else if (cmd == "reiniciar" || cmd == "reboot") {
    Serial.println("Reiniciando...");
    delay(200);
    ESP.restart();
  } else {
    Serial.printf("Comando desconhecido: %s (digite \"ajuda\")\n", cmd.c_str());
  }
}

void consoleLoop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      handle(s_line);
      s_line = "";
    } else if (s_line.length() < 160) {
      s_line += c;
    }
  }
}
