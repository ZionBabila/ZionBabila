// Minimal host-side stand-in for the ESP32 Arduino core, used only by the
// unit tests. Mirrors the parts of the API the firmware uses.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cmath>
#include <math.h>
#include <string>
#include <algorithm>
#include <time.h>
#include <sys/time.h>

using std::min;   // same as the ESP32 core: std::min / std::max
using std::max;

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 3
#endif
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define PROGMEM

// ---- fake clock (the firmware must never touch the host clock) ----
extern double g_now;          // seconds since epoch, with fraction
extern uint32_t g_millis;
inline time_t fake_time(time_t *p) { time_t t = (time_t)g_now; if (p) *p = t; return t; }
inline int fake_settimeofday(const struct timeval *tv, const void *) { g_now = tv->tv_sec + tv->tv_usec / 1e6; return 0; }
inline int fake_gettimeofday(struct timeval *tv, void *) {
  tv->tv_sec = (time_t)g_now; tv->tv_usec = (suseconds_t)((g_now - (double)tv->tv_sec) * 1e6); return 0; }
#define time(p) fake_time(p)
#define settimeofday(a, b) fake_settimeofday(a, b)
#define gettimeofday(a, b) fake_gettimeofday(a, b)

inline uint32_t millis() { return g_millis; }
inline void delay(uint32_t) {}

// ---- GPIO / ADC / PWM ----
extern int g_pin[64];
extern uint32_t g_duty, g_freq, g_mv[64];
inline void pinMode(int, int) {}
inline void digitalWrite(int p, int v) { g_pin[p] = v; }
inline void analogReadResolution(int) {}
inline uint32_t analogReadMilliVolts(int p) { return g_mv[p]; }
#if ESP_ARDUINO_VERSION_MAJOR >= 3
inline bool ledcAttach(uint8_t, uint32_t f, uint8_t) { g_freq = f; return true; }
inline uint32_t ledcChangeFrequency(uint8_t, uint32_t f, uint8_t) { g_freq = f; return f; }
inline bool ledcWrite(uint8_t, uint32_t d) { g_duty = d; return true; }
#else
inline uint32_t ledcSetup(uint8_t, uint32_t f, uint8_t) { g_freq = f; return f; }
inline void ledcAttachPin(uint8_t, uint8_t) {}
inline uint32_t ledcChangeFrequency(uint8_t, uint32_t f, uint8_t) { g_freq = f; return f; }
inline void ledcWrite(uint8_t, uint32_t d) { g_duty = d; }
#endif

// ---- String (subset of Arduino WString) ----
class String {
 public:
  std::string s;
  String() {}
  String(const char *c) : s(c ? c : "") {}
  String(const std::string &x) : s(x) {}
  explicit String(char c) : s(1, c) {}
  explicit String(unsigned char v) : s(std::to_string(v)) {}
  explicit String(int v) : s(std::to_string(v)) {}
  explicit String(unsigned int v) : s(std::to_string(v)) {}
  explicit String(long v) : s(std::to_string(v)) {}
  explicit String(unsigned long v) : s(std::to_string(v)) {}
  String(float v, unsigned int dec) { char b[40]; snprintf(b, sizeof b, "%.*f", (int)dec, v); s = b; }
  String(double v, unsigned int dec) { char b[40]; snprintf(b, sizeof b, "%.*f", (int)dec, v); s = b; }
  unsigned int length() const { return (unsigned)s.size(); }
  const char *c_str() const { return s.c_str(); }
  bool reserve(unsigned int n) { s.reserve(n); return true; }
  void trim() { size_t a = s.find_first_not_of(" \t\r\n"); size_t b = s.find_last_not_of(" \t\r\n");
                s = a == std::string::npos ? "" : s.substr(a, b - a + 1); }
  int indexOf(char c, unsigned int from = 0) const { size_t r = s.find(c, from); return r == std::string::npos ? -1 : (int)r; }
  String substring(unsigned int a, unsigned int b) const { if (a > s.size()) a = s.size(); if (b > s.size()) b = s.size();
                                                           return b <= a ? String("") : String(s.substr(a, b - a)); }
  long toInt() const { return atol(s.c_str()); }
  const char *begin() const { return s.c_str(); }
  const char *end() const { return s.c_str() + s.size(); }
  String &operator+=(const String &o) { s += o.s; return *this; }
  String &operator+=(const char *o) { s += o; return *this; }
  String &operator+=(char c) { s += c; return *this; }
  bool operator==(const char *o) const { return s == o; }
  bool operator==(const String &o) const { return s == o.s; }
};
inline String operator+(const String &a, const String &b) { return String(a.s + b.s); }
inline String operator+(const char *a, const String &b) { return String(std::string(a) + b.s); }
inline String operator+(const String &a, const char *b) { return String(a.s + b); }

// ---- Serial / ESP ----
struct SerialStub {
  void begin(int) {}
  void printf(const char *, ...) {}
  void println(const char * = "") {}
};
extern SerialStub Serial;
struct EspStub { int restarts = 0; void restart() { restarts++; } };
extern EspStub ESP;
