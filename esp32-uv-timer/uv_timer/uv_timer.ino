/*
 * UV LED Weekly Timer - ESP32 Super Mini (ESP32-C3)
 *
 * Built-in web page (Hebrew) for setting weekly time slots.
 * Each slot: days of week, start time, end time, brightness (PWM).
 * Settings are saved to flash (Preferences) and survive power loss.
 *
 * Arduino IDE: board "ESP32C3 Dev Module", "USB CDC On Boot: Enabled".
 * Works with ESP32 Arduino core 2.x and 3.x.
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <time.h>
#include <sys/time.h>
#include "page.h"

// ---------- Hardware ----------
// GPIO4 is safe on the C3 Super Mini. Avoid GPIO2/8/9 (strapping pins).
const int PWM_PIN = 4;
const int STATUS_LED_PIN = 8;      // on-board blue LED, active LOW
const int PWM_BITS = 10;           // duty 0..1023

// ---------- Network ----------
const char *AP_SSID = "UV-Timer";
const char *AP_PASS = "uvtimer123"; // min 8 chars. Change it.
const char *HOSTNAME = "uvtimer";   // http://uvtimer.local
const char *DEFAULT_TZ = "IST-2IDT,M3.4.4/26,M10.5.0"; // Israel

// ---------- Schedule ----------
const int MAX_SLOTS = 16;

struct Slot {
  uint8_t days;     // bit0=Sunday ... bit6=Saturday
  uint16_t start;   // minutes from midnight 0..1439
  uint16_t end;     // minutes from midnight 0..1439 (end < start = crosses midnight)
  uint8_t level;    // 0..100 %
  uint8_t enabled;
};

enum Mode : uint8_t { MODE_AUTO = 0, MODE_ON = 1, MODE_OFF = 2 };

Slot slots[MAX_SLOTS];
int slotCount = 0;
Mode mode = MODE_AUTO;
uint8_t manualLevel = 100;
uint32_t pwmFreq = 1000;
bool pwmInvert = false;
uint8_t maxLevel = 100;           // global limit, protects the LED
String tzString = DEFAULT_TZ;
String wifiSsid, wifiPass;

Preferences prefs;
WebServer server(80);

bool apMode = false;
float currentDuty = 0;            // 0..100, ramps toward target
uint8_t targetLevel = 0;

// ---------- Storage ----------
void loadSettings() {
  prefs.begin("uvtimer", true);
  slotCount = prefs.getBytes("slots", slots, sizeof(slots)) / sizeof(Slot);
  if (slotCount > MAX_SLOTS) slotCount = 0;
  mode = (Mode)prefs.getUChar("mode", MODE_AUTO);
  manualLevel = prefs.getUChar("mlevel", 100);
  pwmFreq = prefs.getUInt("freq", 1000);
  pwmInvert = prefs.getBool("invert", false);
  maxLevel = prefs.getUChar("maxlvl", 100);
  tzString = prefs.getString("tz", DEFAULT_TZ);
  wifiSsid = prefs.getString("ssid", "");
  wifiPass = prefs.getString("pass", "");
  prefs.end();
  if (mode > MODE_OFF) mode = MODE_AUTO;
  if (pwmFreq < 100 || pwmFreq > 40000) pwmFreq = 1000;
}

void saveSlots() {
  prefs.begin("uvtimer", false);
  prefs.putBytes("slots", slots, slotCount * sizeof(Slot));
  prefs.end();
}

void saveMode() {
  prefs.begin("uvtimer", false);
  prefs.putUChar("mode", mode);
  prefs.putUChar("mlevel", manualLevel);
  prefs.end();
}

// ---------- PWM ----------
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

void pwmWritePercent(float pct) {
  uint32_t maxDuty = (1 << PWM_BITS) - 1;
  uint32_t duty = (uint32_t)(pct * maxDuty / 100.0f + 0.5f);
  if (pwmInvert) duty = maxDuty - duty;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PWM_PIN, duty);
#else
  ledcWrite(0, duty);
#endif
}

// ---------- Time ----------
bool timeValid() {
  return time(nullptr) > 1700000000; // after Nov 2023 = clock was set
}

bool slotActive(const Slot &s, int wday, int minutes) {
  if (!s.enabled) return false;
  int yday = (wday + 6) % 7;
  bool today = s.days & (1 << wday);
  bool yesterday = s.days & (1 << yday);
  if (s.start == s.end) return today;                       // whole day
  if (s.start < s.end) return today && minutes >= s.start && minutes < s.end;
  // crosses midnight, e.g. 22:00 -> 06:00
  return (today && minutes >= s.start) || (yesterday && minutes < s.end);
}

uint8_t computeTarget() {
  uint8_t lvl = 0;
  if (mode == MODE_ON) lvl = manualLevel;
  else if (mode == MODE_AUTO && timeValid()) {
    struct tm t;
    time_t now = time(nullptr);
    localtime_r(&now, &t);
    int minutes = t.tm_hour * 60 + t.tm_min;
    for (int i = 0; i < slotCount; i++)
      if (slotActive(slots[i], t.tm_wday, minutes) && slots[i].level > lvl)
        lvl = slots[i].level;
  }
  // no valid time in AUTO mode -> stay OFF (safe)
  return min(lvl, maxLevel);
}

// ---------- Helpers ----------
String jsonEscape(const String &s) {
  String o;
  for (char c : s) {
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c < 0x20) o += ' ';
    else o += c;
  }
  return o;
}

// ---------- Web handlers ----------
void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleState() {
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);

  String j = "{";
  j += "\"time\":\"" + String(buf) + "\",";
  j += "\"wday\":" + String(t.tm_wday) + ",";
  j += "\"timeValid\":" + String(timeValid() ? "true" : "false") + ",";
  j += "\"mode\":" + String(mode) + ",";
  j += "\"manualLevel\":" + String(manualLevel) + ",";
  j += "\"output\":" + String((int)(currentDuty + 0.5f)) + ",";
  j += "\"freq\":" + String(pwmFreq) + ",";
  j += "\"invert\":" + String(pwmInvert ? "true" : "false") + ",";
  j += "\"maxLevel\":" + String(maxLevel) + ",";
  j += "\"tz\":\"" + jsonEscape(tzString) + "\",";
  j += "\"ssid\":\"" + jsonEscape(wifiSsid) + "\",";
  j += "\"ap\":" + String(apMode ? "true" : "false") + ",";
  j += "\"ip\":\"" + (apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "\",";
  j += "\"rssi\":" + String(apMode ? 0 : WiFi.RSSI()) + ",";
  j += "\"slots\":[";
  for (int i = 0; i < slotCount; i++) {
    if (i) j += ",";
    j += "{\"d\":" + String(slots[i].days) + ",\"s\":" + String(slots[i].start) +
         ",\"e\":" + String(slots[i].end) + ",\"l\":" + String(slots[i].level) +
         ",\"on\":" + String(slots[i].enabled) + "}";
  }
  j += "]}";
  server.send(200, "application/json", j);
}

// body field "s" = "days,start,end,level,enabled;days,start,end,level,enabled;..."
void handleSlots() {
  String s = server.arg("s");
  Slot tmp[MAX_SLOTS];
  int n = 0, pos = 0;
  while (pos < (int)s.length() && n < MAX_SLOTS) {
    int sep = s.indexOf(';', pos);
    if (sep < 0) sep = s.length();
    String item = s.substring(pos, sep);
    pos = sep + 1;
    int v[5], k = 0, p = 0;
    while (k < 5) {
      int c = item.indexOf(',', p);
      if (c < 0) c = item.length();
      v[k++] = item.substring(p, c).toInt();
      if (c >= (int)item.length()) break;
      p = c + 1;
    }
    if (k < 5) continue;
    tmp[n].days = v[0] & 0x7F;
    tmp[n].start = constrain(v[1], 0, 1439);
    tmp[n].end = constrain(v[2], 0, 1439);
    tmp[n].level = constrain(v[3], 0, 100);
    tmp[n].enabled = v[4] ? 1 : 0;
    n++;
  }
  memcpy(slots, tmp, sizeof(Slot) * n);
  slotCount = n;
  saveSlots();
  server.send(200, "text/plain", "OK");
}

void handleMode() {
  if (server.hasArg("mode")) mode = (Mode)constrain(server.arg("mode").toInt(), 0, 2);
  if (server.hasArg("level")) manualLevel = constrain(server.arg("level").toInt(), 0, 100);
  saveMode();
  server.send(200, "text/plain", "OK");
}

// browser sends its own clock (UTC epoch seconds) - works without internet
void handleTime() {
  long long epoch = atoll(server.arg("epoch").c_str());
  if (epoch > 1700000000LL) {
    struct timeval tv = { (time_t)epoch, 0 };
    settimeofday(&tv, nullptr);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "bad epoch");
  }
}

void handleSettings() {
  prefs.begin("uvtimer", false);
  if (server.hasArg("freq")) {
    uint32_t f = constrain(server.arg("freq").toInt(), 100, 40000);
    if (f != pwmFreq) { pwmFreq = f; pwmSetFreq(pwmFreq); }
    prefs.putUInt("freq", pwmFreq);
  }
  if (server.hasArg("invert")) {
    pwmInvert = server.arg("invert") == "1";
    prefs.putBool("invert", pwmInvert);
  }
  if (server.hasArg("maxLevel")) {
    maxLevel = constrain(server.arg("maxLevel").toInt(), 0, 100);
    prefs.putUChar("maxlvl", maxLevel);
  }
  if (server.hasArg("tz") && server.arg("tz").length() > 2) {
    tzString = server.arg("tz");
    prefs.putString("tz", tzString);
    setenv("TZ", tzString.c_str(), 1);
    tzset();
  }
  prefs.end();
  server.send(200, "text/plain", "OK");
}

void handleWifi() {
  prefs.begin("uvtimer", false);
  prefs.putString("ssid", server.arg("ssid"));
  prefs.putString("pass", server.arg("pass"));
  prefs.end();
  server.send(200, "text/plain", "OK - restarting");
  delay(500);
  ESP.restart();
}

// ---------- WiFi ----------
void startWifi() {
  WiFi.setHostname(HOSTNAME);
  if (wifiSsid.length()) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
    // Many C3 Super Mini boards connect much better with reduced TX power
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    Serial.printf("Connecting to %s", wifiSsid.c_str());
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
  }
  if (WiFi.status() == WL_CONNECTED) {
    apMode = false;
    WiFi.setAutoReconnect(true);
    Serial.printf("Connected. IP: %s\n", WiFi.localIP().toString().c_str());
    configTzTime(tzString.c_str(), "pool.ntp.org", "time.google.com");
  } else {
    apMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    setenv("TZ", tzString.c_str(), 1);
    tzset();
    Serial.printf("AP mode: %s / %s  -> http://%s\n", AP_SSID, AP_PASS,
                  WiFi.softAPIP().toString().c_str());
  }
  if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
}

// ---------- Main ----------
void setup() {
  // Make sure the LED is OFF from the first moment
  pinMode(PWM_PIN, OUTPUT);
  digitalWrite(PWM_PIN, LOW);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, HIGH);

  Serial.begin(115200);
  delay(300);
  loadSettings();
  pwmBegin();
  pwmWritePercent(0);

  startWifi();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/slots", HTTP_POST, handleSlots);
  server.on("/api/mode", HTTP_POST, handleMode);
  server.on("/api/time", HTTP_POST, handleTime);
  server.on("/api/settings", HTTP_POST, handleSettings);
  server.on("/api/wifi", HTTP_POST, handleWifi);
  server.onNotFound([]() { server.sendHeader("Location", "/"); server.send(302); });
  server.begin();
}

void loop() {
  server.handleClient();

  static uint32_t lastCheck = 0, lastRamp = 0;
  uint32_t ms = millis();

  if (ms - lastCheck >= 1000) {
    lastCheck = ms;
    targetLevel = computeTarget();
    digitalWrite(STATUS_LED_PIN, targetLevel ? LOW : HIGH); // blue LED mirrors UV state
  }

  // soft ramp: full 0->100% in about 2 seconds
  if (ms - lastRamp >= 20) {
    lastRamp = ms;
    if (currentDuty < targetLevel) currentDuty = min((float)targetLevel, currentDuty + 1.0f);
    else if (currentDuty > targetLevel) currentDuty = max((float)targetLevel, currentDuty - 1.0f);
    pwmWritePercent(currentDuty);
  }
}
