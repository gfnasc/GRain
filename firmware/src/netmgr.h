#pragma once

#include <Arduino.h>

struct NetStatus {
  bool connected = false;
  bool portalActive = false;  // rede de configuração aberta
  bool timeSynced = false;
  String apName;              // nome da rede de configuração
  String apPass;              // senha da rede de configuração
  String ssid;
  String ip;
  int rssi = 0;
};

// Inicia Wi-Fi (WiFiManager não bloqueante), mDNS e NTP.
// Sem Wi-Fi salvo, abre a rede de configuração "GWeather-Setup".
void netBegin();

// Chamar no loop(): atende o portal, reconecta e acompanha a hora.
void netLoop();

const NetStatus &netStatus();

// Hora local formatada; retorna false se a hora ainda não foi sincronizada.
bool netLocalTime(char *out, size_t n, const char *fmt);

// Esquece o Wi-Fi salvo e reinicia (abre a rede de configuração no boot).
void netForgetWifi();

// Abre a rede de configuração sem esquecer o Wi-Fi atual.
void netOpenPortal();
