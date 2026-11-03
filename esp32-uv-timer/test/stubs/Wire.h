#pragma once
#include "Arduino.h"
// Simulated I2C bus with an optional DS3231 at 0x68.
struct TwoWire {
  bool present = false; uint8_t reg[0x13] = {}; uint8_t ptr = 0;
  bool first = false; uint8_t addr = 0; uint8_t rx[16]; int rxn = 0, rxi = 0;
  bool begin(int, int, uint32_t = 0) { return true; }
  void beginTransmission(int a) { addr = a; first = true; }
  size_t write(uint8_t b) {
    if (addr != 0x68 || !present) return 1;
    if (first) { ptr = b; first = false; } else if (ptr < sizeof reg) reg[ptr++] = b;
    return 1; }
  size_t write(int b) { return write((uint8_t)b); }
  uint8_t endTransmission() { return (addr == 0x68 && present) ? 0 : 2; }
  uint8_t requestFrom(uint8_t a, uint8_t n) {
    if (a != 0x68 || !present) return 0;
    rxn = n; rxi = 0; for (int i = 0; i < n; i++) rx[i] = reg[(ptr + i) % sizeof reg];
    return n; }
  int read() { return rxi < rxn ? rx[rxi++] : -1; }
};
extern TwoWire Wire;
