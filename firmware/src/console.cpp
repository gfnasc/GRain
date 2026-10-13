#include "console.h"

#include <Arduino.h>
#include <LittleFS.h>

#include "display.h"
#include "netmgr.h"
#include "sensors.h"
#include "settings.h"
#include "uplink.h"

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
      "  nuvem                 estado do envio para a nuvem\n"
      "  nuvem url <https://...>  endereço do painel na Vercel\n"
      "  nuvem token <token>   token da estação (gerado com pnpm device:token)\n"
      "  nuvem enviar          envia a fila agora\n"
      "  nuvem limpar          descarta a fila de médias\n"
      "  nuvem desligar        apaga endereço e token\n"
      "  tela ligar|desligar   liga ou desliga o display (até reiniciar)\n"
      "  carrossel pausar|retomar\n"
      "  pagina proxima|anterior|<n>  navega entre as páginas do display\n"
      "  wifi portal           abre a rede de configuração sem esquecer o Wi-Fi atual\n"
      "  wifi esquecer         apaga o Wi-Fi salvo e reinicia\n"
      "  reiniciar             reinicia a placa\n");
}

static void printCloud() {
  const UplinkStatus &u = uplinkStatus();
  char t[32] = "nunca";
  if (u.lastOkEpoch) {
    time_t e = u.lastOkEpoch;
    struct tm lt;
    localtime_r(&e, &lt);
    strftime(t, sizeof t, "%d/%m %H:%M", &lt);
  }
  Serial.printf("Nuvem   : %s\n", u.configured ? u.url.c_str() : "não configurada (nuvem url / nuvem token)");
  Serial.printf("          fila %u médias; último envio ok: %s; cópia da configuração %s", u.queue, t,
                u.configPending ? "pendente" : (u.configSentEpoch ? "enviada" : "–"));
  if (u.lastError.length()) Serial.printf("; último erro: %s", u.lastError.c_str());
  Serial.println();
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
  printCloud();
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
  } else if (cmd == "nuvem" && rest.isEmpty()) {
    printCloud();
  } else if (cmd == "nuvem" && rest.startsWith("url ")) {
    String err;
    if (uplinkSetUrl(rest.substring(4), err)) Serial.printf("Endereço salvo: %s\n", uplinkStatus().url.c_str());
    else Serial.printf("Não alterado: %s\n", err.c_str());
  } else if (cmd == "nuvem" && rest.startsWith("token ")) {
    String err, tok = rest.substring(6);
    tok.trim();
    if (uplinkSetToken(tok, err)) Serial.println("Token salvo na NVS.");
    else Serial.printf("Não alterado: %s\n", err.c_str());
  } else if (cmd == "nuvem" && rest == "enviar") {
    uplinkSendNow();
    Serial.println("Envio solicitado.");
  } else if (cmd == "nuvem" && rest == "limpar") {
    Serial.printf("%u médias descartadas.\n", uplinkClearQueue());
  } else if (cmd == "nuvem" && rest == "desligar") {
    uplinkDisable();
    Serial.println("Envio para a nuvem desligado (endereço e token apagados).");
  } else if (cmd == "tela" && (rest == "ligar" || rest == "desligar")) {
    displaySetPower(rest == "ligar");
    Serial.printf("Tela %s.\n", rest == "ligar" ? "ligada" : "desligada");
  } else if (cmd == "carrossel" && (rest == "pausar" || rest == "retomar")) {
    displaySetPaused(rest == "pausar");
    Serial.printf("Carrossel %s.\n", rest == "pausar" ? "pausado" : "retomado");
  } else if (cmd == "pagina" && rest.length()) {
    if (rest == "proxima") displayStep(1);
    else if (rest == "anterior") displayStep(-1);
    else if (!displayGoto(rest.toInt() - 1)) {
      Serial.printf("Use proxima, anterior ou um número de 1 a %d.\n", displayState().count);
      return;
    }
    DisplayState d = displayState();
    Serial.printf("Página %d de %d: %s\n", d.page + 1, d.count, pageTypeName(settings().pages[d.page].type));
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
