#pragma once
#include "Arduino.h"
struct IPAddress { uint8_t a[4]; IPAddress(int x = 0, int y = 0, int z = 0, int w = 0) : a{(uint8_t)x, (uint8_t)y, (uint8_t)z, (uint8_t)w} {} };
enum { WIFI_AP = 2 };
enum wifi_power_t { WIFI_POWER_8_5dBm = 34 };
struct WiFiStub {
  std::string ssid, pass; bool open = false;
  void mode(int) {}
  bool softAPConfig(IPAddress, IPAddress, IPAddress) { return true; }
  bool softAP(const char *s, const char *p) { ssid = s; open = !p; pass = p ? p : ""; return true; }
  bool setTxPower(wifi_power_t) { return true; }
};
extern WiFiStub WiFi;
