/*
 * UV LED Weekly Timer - ESP32-C3 Super Mini (offline, access point only)
 *
 * The board creates its own Wi-Fi network. Connect with a phone and the
 * control page opens automatically (captive portal), or browse to
 * http://192.168.4.1
 *
 * - Clock is set from the phone (one tap) or entered manually.
 * - Optional DS3231 clock module keeps time through power cuts.
 * - Events: days, start time, duration, brightness, instant/fade on & off.
 * - Optional NTC thermistors limit brightness when the heatsink gets hot.
 *
 * Arduino IDE: board "ESP32C3 Dev Module", "USB CDC On Boot: Enabled".
 * Works with ESP32 Arduino core 2.x and 3.x. No extra libraries needed.
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <time.h>
#include <sys/time.h>
#include "page.h"

// ---------- Pins ----------
// Avoid GPIO2/8/9 for external parts (boot strapping pins).
const int PWM_PIN = 4;             // to PWM input of both LED drivers
const int STATUS_LED_PIN = 8;      // on-board blue LED, active LOW
const int NTC_PINS[2] = {3, 1};    // thermistor 1 / thermistor 2 (ADC)
const int I2C_SDA = 5;             // DS3231 SDA (optional)
const int I2C_SCL = 6;             // DS3231 SCL (optional)

// ---------- PWM ----------
const int PWM_BITS = 12;           // 0..4095, smooth fades at low brightness
const uint32_t PWM_FREQ_MAX = 19000; // max for 12 bits on ESP32-C3

// ---------- Thermistor (NTC 100K B3950 + 100k resistor to 3.3V) ----------
const float NTC_R25 = 100000.0f;
const float NTC_BETA = 3950.0f;
const float NTC_RFIXED = 100000.0f;
const uint8_t FAULT_CAP = 30;      // max % while a sensor is broken

// ---------- Access point defaults ----------
const char *DEFAULT_SSID = "UV-Timer";
const char *DEFAULT_PASS = "uvtimer123"; // 8+ chars, or empty for open network

// ---------- Events ----------
const int MAX_EVENTS = 20;

struct Event {
  uint8_t days;      // bit0 = Sunday ... bit6 = Saturday
  uint8_t level;     // brightness 1..100 %
  uint16_t start;    // minutes after midnight 0..1439
  uint16_t duration; // minutes 1..1440 (may run past midnight)
  uint8_t fadeIn;    // 0 = instant on, 1 = fade in
  uint8_t fadeOut;   // 0 = instant off, 1 = fade out
  uint16_t fadeSec;  // fade length in seconds 1..3600
  uint8_t enabled;
};

// Older firmware stored the fade in minutes; used once to convert saved events.
struct EventV1 {
  uint8_t days, level;
  uint16_t start, duration;
  uint8_t fadeIn, fadeOut, fadeMin, enabled;
};

enum Mode : uint8_t { MODE_AUTO = 0, MODE_ON = 1, MODE_OFF = 2 };

Event events[MAX_EVENTS];
int eventCount = 0;
Mode mode = MODE_AUTO;
uint8_t manualLevel = 100;
uint8_t maxLevel = 100;
uint32_t pwmFreq = 1000;
bool pwmInvert = false;
uint8_t sensorCount = 0;
uint8_t tempLimit = 50;
String apSsid, apPass;

Preferences prefs;
WebServer server(80);
DNSServer dns;

bool rtcFound = false;
float output = 0;                  // current output %, after all limits
float temps[2] = {NAN, NAN};
float thermalCap = 100;
bool sensorFault = false;

// ================= Storage =================
void saveEvents() {
  prefs.begin("uvtimer2", false);
  prefs.putBytes("events2", events, eventCount * sizeof(Event));
  prefs.end();
}

void loadSettings() {
  prefs.begin("uvtimer2", true);
  eventCount = prefs.getBytes("events2", events, sizeof(events)) / sizeof(Event);
  bool migrate = !prefs.isKey("events2") && prefs.isKey("events");
  if (migrate) {                                 // convert minutes -> seconds
    EventV1 old[MAX_EVENTS];
    int n = prefs.getBytes("events", old, sizeof(old)) / sizeof(EventV1);
    for (int i = 0; i < n && i < MAX_EVENTS; i++) {
      events[i] = {old[i].days, old[i].level, old[i].start, old[i].duration,
                   old[i].fadeIn, old[i].fadeOut, (uint16_t)(old[i].fadeMin * 60), old[i].enabled};
    }
    eventCount = min(n, MAX_EVENTS);
  }
  mode = (Mode)prefs.getUChar("mode", MODE_AUTO);
  manualLevel = prefs.getUChar("mlevel", 100);
  maxLevel = prefs.getUChar("maxlvl", 100);
  pwmFreq = prefs.getUInt("freq", 1000);
  pwmInvert = prefs.getBool("invert", false);
  sensorCount = prefs.getUChar("sensors", 0);
  tempLimit = prefs.getUChar("tlimit", 50);
  apSsid = prefs.getString("ssid", DEFAULT_SSID);
  apPass = prefs.getString("pass", DEFAULT_PASS);
  prefs.end();
  if (eventCount > MAX_EVENTS) eventCount = 0;
  if (migrate) saveEvents();
  if (mode > MODE_OFF) mode = MODE_AUTO;
  if (pwmFreq < 100 || pwmFreq > PWM_FREQ_MAX) pwmFreq = 1000;
  if (sensorCount > 2) sensorCount = 0;
  if (tempLimit < 30 || tempLimit > 90) tempLimit = 50;
  if (!apSsid.length()) apSsid = DEFAULT_SSID;
}

// ================= Clock =================
// The system clock holds LOCAL time (timezone UTC0), so no timezone setup
// is needed: the phone sends its local time.
bool timeSet() { return time(nullptr) > 1700000000; }

uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

bool rtcRead() {
  Wire.beginTransmission(0x68);
  Wire.write(0x0F);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((uint8_t)0x68, (uint8_t)1) != 1) return false;
  if (Wire.read() & 0x80) return false;          // oscillator stopped: time invalid

  Wire.beginTransmission(0x68);
  Wire.write(0x00);
  Wire.endTransmission();
  if (Wire.requestFrom((uint8_t)0x68, (uint8_t)7) != 7) return false;
  struct tm t = {};
  t.tm_sec = bcd2dec(Wire.read() & 0x7F);
  t.tm_min = bcd2dec(Wire.read());
  t.tm_hour = bcd2dec(Wire.read() & 0x3F);
  Wire.read();                                   // day of week (unused)
  t.tm_mday = bcd2dec(Wire.read());
  t.tm_mon = bcd2dec(Wire.read() & 0x1F) - 1;
  t.tm_year = bcd2dec(Wire.read()) + 100;
  time_t epoch = mktime(&t);
  if (epoch < 1700000000) return false;
  struct timeval tv = {epoch, 0};
  settimeofday(&tv, nullptr);
  return true;
}

void rtcWrite(time_t epoch) {
  struct tm t;
  gmtime_r(&epoch, &t);
  Wire.beginTransmission(0x68);
  Wire.write(0x00);
  Wire.write(dec2bcd(t.tm_sec));
  Wire.write(dec2bcd(t.tm_min));
  Wire.write(dec2bcd(t.tm_hour));
  Wire.write(t.tm_wday + 1);
  Wire.write(dec2bcd(t.tm_mday));
  Wire.write(dec2bcd(t.tm_mon + 1));
  Wire.write(dec2bcd(t.tm_year % 100));
  Wire.endTransmission();
  Wire.beginTransmission(0x68);                  // clear "oscillator stopped" flag
  Wire.write(0x0F);
  Wire.write(0x00);
  Wire.endTransmission();
}

void rtcBegin() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.beginTransmission(0x68);
  rtcFound = Wire.endTransmission() == 0;
  if (rtcFound) {
    bool ok = rtcRead();
    Serial.printf("DS3231 found, time %s\n", ok ? "loaded" : "not set yet");
  }
}

// ================= PWM =================
void pwmBegin() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PWM_PIN, pwmFreq, PWM_BITS);
#else
  ledcSetup(0, pwmFreq, PWM_BITS);
  ledcAttachPin(PWM_PIN, 0);
#endif
}

void pwmSetFreq(uint32_t f) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcChangeFrequency(PWM_PIN, f, PWM_BITS);
#else
  ledcChangeFrequency(0, f, PWM_BITS);
#endif
}

void pwmWrite(float pct) {
  const uint32_t maxDuty = (1 << PWM_BITS) - 1;
  uint32_t duty = (uint32_t)(pct * maxDuty / 100.0f + 0.5f);
  if (duty > maxDuty) duty = maxDuty;
  if (pwmInvert) duty = maxDuty - duty;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PWM_PIN, duty);
#else
  ledcWrite(0, duty);
#endif
}

// ================= Temperature =================
float readNtc(int pin) {
  uint32_t mv = 0;
  for (int i = 0; i < 16; i++) mv += analogReadMilliVolts(pin);
  mv /= 16;
  if (mv < 40 || mv > 2700) return NAN;          // short / disconnected
  float r = NTC_RFIXED * mv / (3300.0f - mv);
  float k = 1.0f / (1.0f / 298.15f + logf(r / NTC_R25) / NTC_BETA);
  return k - 273.15f;
}

float thermalLimit() {
  if (sensorCount == 0) return 100;
  return sensorFault ? min(thermalCap, (float)FAULT_CAP) : thermalCap;
}

void updateThermal() {                           // every second
  static uint8_t tick = 0;
  if (sensorCount == 0) {
    temps[0] = temps[1] = NAN;
    sensorFault = false;
    thermalCap = 100;
    return;
  }
  float maxT = NAN;
  sensorFault = false;
  for (int i = 0; i < 2; i++) {
    temps[i] = (i < sensorCount) ? readNtc(NTC_PINS[i]) : NAN;
    if (i < sensorCount && isnan(temps[i])) sensorFault = true;
    if (!isnan(temps[i]) && (isnan(maxT) || temps[i] > maxT)) maxT = temps[i];
  }
  if (isnan(maxT)) return;
  if (maxT > tempLimit + 15) { thermalCap = 0; return; }   // emergency off
  if (++tick < 5) return;
  tick = 0;
  if (maxT > tempLimit) thermalCap = max(0.0f, thermalCap - 5);
  else if (maxT < tempLimit - 2) thermalCap = min(100.0f, thermalCap + 2);
}

// ================= Schedule =================
// Brightness of one event at this moment (0 if not running).
// secOfDay includes fractions of a second so short fades are smooth.
float eventLevel(const Event &e, int wday, double secOfDay) {
  if (!e.enabled || !e.duration) return 0;
  float best = 0;
  long dur = e.duration * 60L;
  for (int back = 0; back < 2; back++) {         // started today or yesterday
    int d = (wday + 7 - back) % 7;
    if (!(e.days & (1 << d))) continue;
    float el = (float)(secOfDay + back * 86400.0 - e.start * 60.0);
    if (el < 0 || el >= dur) continue;
    float fade = e.fadeSec;
    int fades = (e.fadeIn ? 1 : 0) + (e.fadeOut ? 1 : 0);
    if (fades && fade * fades > dur) fade = (float)dur / fades;
    float env = 1;
    if (e.fadeIn && fade > 0) env = min(env, el / fade);
    if (e.fadeOut && fade > 0) env = min(env, (dur - el) / fade);
    best = max(best, e.level * env * env);       // squared = smoother to the eye
  }
  return best;
}

float scheduleLevel() {
  if (!timeSet()) return 0;                      // unknown time -> stay off
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  time_t now = tv.tv_sec;
  struct tm t;
  gmtime_r(&now, &t);
  double sec = t.tm_hour * 3600.0 + t.tm_min * 60.0 + t.tm_sec + tv.tv_usec / 1e6;
  float lvl = 0;
  for (int i = 0; i < eventCount; i++) lvl = max(lvl, eventLevel(events[i], t.tm_wday, sec));
  return lvl;
}

float computeOutput() {
  float lvl = 0;
  if (mode == MODE_ON) lvl = manualLevel;
  else if (mode == MODE_AUTO) lvl = scheduleLevel();
  return min(min(lvl, (float)maxLevel), thermalLimit());
}

// ================= Web =================
String jsonEscape(const String &s) {
  String o;
  for (char c : s) {
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c < 0x20) o += ' ';
    else o += c;
  }
  return o;
}

void handleRoot() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleState() {
  String j = "{";
  j += "\"now\":" + String((long)time(nullptr)) + ",";
  j += "\"timeSet\":" + String(timeSet() ? "true" : "false") + ",";
  j += "\"rtc\":" + String(rtcFound ? "true" : "false") + ",";
  j += "\"mode\":" + String(mode) + ",";
  j += "\"manualLevel\":" + String(manualLevel) + ",";
  j += "\"output\":" + String(output, 1) + ",";
  j += "\"maxLevel\":" + String(maxLevel) + ",";
  j += "\"freq\":" + String(pwmFreq) + ",";
  j += "\"invert\":" + String(pwmInvert ? "true" : "false") + ",";
  j += "\"sensors\":" + String(sensorCount) + ",";
  j += "\"tlimit\":" + String(tempLimit) + ",";
  j += "\"tcap\":" + String((int)thermalLimit()) + ",";
  j += "\"fault\":" + String(sensorFault ? "true" : "false") + ",";
  j += "\"temps\":[";
  for (int i = 0; i < sensorCount; i++) {
    if (i) j += ",";
    j += isnan(temps[i]) ? String("null") : String(temps[i], 1);
  }
  j += "],\"ssid\":\"" + jsonEscape(apSsid) + "\",";
  j += "\"events\":[";
  for (int i = 0; i < eventCount; i++) {
    const Event &e = events[i];
    if (i) j += ",";
    j += "{\"d\":" + String(e.days) + ",\"s\":" + String(e.start) + ",\"u\":" + String(e.duration) +
         ",\"l\":" + String(e.level) + ",\"fi\":" + String(e.fadeIn) + ",\"fo\":" + String(e.fadeOut) +
         ",\"f\":" + String(e.fadeSec) + ",\"on\":" + String(e.enabled) + "}";
  }
  j += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

// body "e" = "days,start,duration,level,fadeIn,fadeOut,fadeSec,enabled;..."
void handleEvents() {
  String s = server.arg("e");
  Event tmp[MAX_EVENTS];
  int n = 0, pos = 0;
  while (pos < (int)s.length() && n < MAX_EVENTS) {
    int sep = s.indexOf(';', pos);
    if (sep < 0) sep = s.length();
    String item = s.substring(pos, sep);
    pos = sep + 1;
    long v[8];
    int k = 0, p = 0;
    while (k < 8 && p <= (int)item.length()) {
      int c = item.indexOf(',', p);
      if (c < 0) c = item.length();
      v[k++] = item.substring(p, c).toInt();
      p = c + 1;
    }
    if (k < 8) continue;
    Event &e = tmp[n++];
    e.days = v[0] & 0x7F;
    e.start = constrain(v[1], 0, 1439);
    e.duration = constrain(v[2], 1, 1440);
    e.level = constrain(v[3], 1, 100);
    e.fadeIn = v[4] ? 1 : 0;
    e.fadeOut = v[5] ? 1 : 0;
    e.fadeSec = constrain(v[6], 1, 3600);
    e.enabled = v[7] ? 1 : 0;
  }
  memcpy(events, tmp, sizeof(Event) * n);
  eventCount = n;
  saveEvents();
  server.send(200, "text/plain", "OK");
}

void handleMode() {
  if (server.hasArg("mode")) mode = (Mode)constrain(server.arg("mode").toInt(), 0, 2);
  if (server.hasArg("level")) manualLevel = constrain(server.arg("level").toInt(), 0, 100);
  prefs.begin("uvtimer2", false);
  prefs.putUChar("mode", mode);
  prefs.putUChar("mlevel", manualLevel);
  prefs.end();
  server.send(200, "text/plain", "OK");
}

// "t" = local time in seconds (phone's local clock, or manual entry)
void handleTime() {
  long long t = atoll(server.arg("t").c_str());
  if (t < 1700000000LL || t > 4000000000LL) { server.send(400, "text/plain", "bad time"); return; }
  struct timeval tv = {(time_t)t, 0};
  settimeofday(&tv, nullptr);
  if (rtcFound) rtcWrite((time_t)t);
  server.send(200, "text/plain", "OK");
}

void handleSettings() {
  prefs.begin("uvtimer2", false);
  if (server.hasArg("maxLevel")) {
    maxLevel = constrain(server.arg("maxLevel").toInt(), 1, 100);
    prefs.putUChar("maxlvl", maxLevel);
  }
  if (server.hasArg("freq")) {
    uint32_t f = constrain(server.arg("freq").toInt(), 100, (long)PWM_FREQ_MAX);
    if (f != pwmFreq) { pwmFreq = f; pwmSetFreq(pwmFreq); }
    prefs.putUInt("freq", pwmFreq);
  }
  if (server.hasArg("invert")) {
    pwmInvert = server.arg("invert") == "1";
    prefs.putBool("invert", pwmInvert);
  }
  if (server.hasArg("sensors")) {
    sensorCount = constrain(server.arg("sensors").toInt(), 0, 2);
    prefs.putUChar("sensors", sensorCount);
  }
  if (server.hasArg("tlimit")) {
    tempLimit = constrain(server.arg("tlimit").toInt(), 30, 90);
    prefs.putUChar("tlimit", tempLimit);
  }
  prefs.end();
  server.send(200, "text/plain", "OK");
}

void handleWifi() {
  String ssid = server.arg("ssid"), pass = server.arg("pass");
  ssid.trim();
  if (!ssid.length() || ssid.length() > 32 || (pass.length() && pass.length() < 8) || pass.length() > 63) {
    server.send(400, "text/plain", "Name 1-32 chars, password empty or 8-63 chars");
    return;
  }
  prefs.begin("uvtimer2", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
  server.send(200, "text/plain", "OK");
  delay(500);
  ESP.restart();
}

// Phones probe these URLs; redirecting them opens the page automatically.
void handleCaptive() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

// ================= Main =================
void setup() {
  pinMode(PWM_PIN, OUTPUT);
  digitalWrite(PWM_PIN, LOW);                    // LED off from the first moment
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, HIGH);

  Serial.begin(115200);
  delay(300);
  setenv("TZ", "UTC0", 1);
  tzset();
  loadSettings();
  pwmBegin();
  pwmWrite(0);
  analogReadResolution(12);
  rtcBegin();

  IPAddress ip(192, 168, 4, 1);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));
  WiFi.softAP(apSsid.c_str(), apPass.length() ? apPass.c_str() : nullptr);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);            // more stable on C3 Super Mini
  dns.start(53, "*", ip);
  Serial.printf("Wi-Fi: %s  ->  http://192.168.4.1\n", apSsid.c_str());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/events", HTTP_POST, handleEvents);
  server.on("/api/mode", HTTP_POST, handleMode);
  server.on("/api/time", HTTP_POST, handleTime);
  server.on("/api/settings", HTTP_POST, handleSettings);
  server.on("/api/wifi", HTTP_POST, handleWifi);
  server.onNotFound(handleCaptive);
  server.begin();
}

void loop() {
  dns.processNextRequest();
  server.handleClient();

  static uint32_t lastSec = 0, lastOut = 0, lastRtc = 0;
  uint32_t ms = millis();

  if (ms - lastSec >= 1000) {
    lastSec = ms;
    updateThermal();
  }
  if (ms - lastOut >= 50) {                      // 20 updates/s = smooth fades
    lastOut = ms;
    output = computeOutput();
    pwmWrite(output);
    digitalWrite(STATUS_LED_PIN, output > 0 ? LOW : HIGH);
  }
  if (rtcFound && ms - lastRtc >= 3600000UL) {   // re-sync from DS3231 hourly
    lastRtc = ms;
    rtcRead();
  }
}
