#pragma once
#include "Arduino.h"
#include <map>
#include <vector>
// In-memory "flash": survives prefs.begin()/end() like real NVS.
extern std::map<std::string, std::vector<uint8_t>> g_nvs;
struct Preferences {
  std::string ns; bool ro = false;
  bool begin(const char *n, bool readOnly) { ns = n; ro = readOnly; return true; }
  void end() {}
  std::string k(const char *key) { return ns + "/" + key; }
  bool isKey(const char *key) { return g_nvs.count(k(key)); }
  size_t putBytes(const char *key, const void *b, size_t n) { if (ro) return 0; auto p = (const uint8_t *)b; g_nvs[k(key)] = std::vector<uint8_t>(p, p + n); return n; }
  size_t getBytes(const char *key, void *b, size_t max) { if (!isKey(key)) return 0; auto &v = g_nvs[k(key)]; if (v.size() > max) return 0; memcpy(b, v.data(), v.size()); return v.size(); }
  template <typename T> size_t putT(const char *key, T v) { return putBytes(key, &v, sizeof v); }
  template <typename T> T getT(const char *key, T d) { T v; return getBytes(key, &v, sizeof v) == sizeof v ? v : d; }
  size_t putUChar(const char *key, uint8_t v) { return putT(key, v); }
  uint8_t getUChar(const char *key, uint8_t d = 0) { return getT(key, d); }
  size_t putUInt(const char *key, uint32_t v) { return putT(key, v); }
  uint32_t getUInt(const char *key, uint32_t d = 0) { return getT(key, d); }
  size_t putBool(const char *key, bool v) { return putT(key, (uint8_t)v); }
  bool getBool(const char *key, bool d = false) { return getT(key, (uint8_t)d); }
  size_t putString(const char *key, const String &v) { return putBytes(key, v.c_str(), v.length() + 1); }
  String getString(const char *key, const String d = String()) { if (!isKey(key)) return d; return String((const char *)g_nvs[k(key)].data()); }
};
