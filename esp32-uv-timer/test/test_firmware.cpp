// Host-side unit tests for uv_timer.ino.
// Build & run: ./run_tests.sh   (needs only g++)
#include "Arduino.h"
#include <map>
#include <vector>

// ---- globals the stubs refer to ----
double g_now = 0;
uint32_t g_millis = 0;
int g_pin[64];
uint32_t g_duty = 0, g_freq = 0, g_mv[64];
std::map<std::string, std::vector<uint8_t>> g_nvs;
SerialStub Serial;
EspStub ESP;

#include "../uv_timer/uv_timer.ino"   // the real firmware

WiFiStub WiFi;
TwoWire Wire;

// ---- tiny test framework ----
static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { if (cond) g_pass++; else { g_fail++; printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define NEAR(a, b, tol) do { double _a = (a), _b = (b); if (fabs(_a - _b) <= (tol)) g_pass++; else { g_fail++; \
  printf("  FAIL %s:%d  %s = %.3f, expected %.3f\n", __FILE__, __LINE__, #a, _a, _b); } } while (0)
#define TEST(name) static void name(); struct name##_r { name##_r() { tests().push_back({#name, name}); } } name##_i; static void name()
static std::vector<std::pair<const char *, void (*)()>> &tests() { static std::vector<std::pair<const char *, void (*)()>> t; return t; }

// ---- helpers ----
// 2026-11-01 is a Sunday. day 0 = Sun ... 6 = Sat.
static double at(int wday, int h, int m, double s = 0) {
  struct tm t = {}; t.tm_year = 126; t.tm_mon = 10; t.tm_mday = 1 + wday; t.tm_hour = h; t.tm_min = m;
  return (double)timegm(&t) + s;
}
static Event steady(uint8_t days, int h, int m, int durMin, int level, bool fi = false, bool fo = false, int fade = 10) {
  Event e = {}; e.days = days; e.start = h * 60 + m; e.duration = durMin; e.level = level;
  e.fadeIn = fi; e.fadeOut = fo; e.fadeSec = fade; e.enabled = 1; return e;
}
static Event interval(uint8_t days, int h, int m, int durMin, int level, int up, int hold, int down, int off) {
  Event e = steady(days, h, m, durMin, level); e.pattern = 1;
  e.upSec = up; e.holdSec = hold; e.downSec = down; e.offSec = off; return e;
}
static void setEvents(std::vector<Event> v) { eventCount = v.size(); for (size_t i = 0; i < v.size(); i++) events[i] = v[i]; }
static float levelAt(double t) { g_now = t; return scheduleLevel(); }
static void resetAll() {
  g_nvs.clear(); g_now = 0; g_millis = 0; eventCount = 0; mode = MODE_AUTO; manualLevel = 100; maxLevel = 100;
  sensorCount = 0; tempLimit = 50; thermalCap = 100; sensorFault = false; pwmInvert = false; pwmFreq = 1000;
  Wire = TwoWire(); server.reset(); ESP.restarts = 0; for (auto &m : g_mv) m = 0; rtcFound = false;
}
static const int MON = 1 << 1, SAT = 1 << 6, ALL = 127;

// ================= schedule: steady =================
TEST(steady_instant_on_off) {
  setEvents({steady(MON, 18, 0, 60, 80)});
  NEAR(levelAt(at(1, 17, 59, 59.9)), 0, 0);
  NEAR(levelAt(at(1, 18, 0)), 80, 0.01);
  NEAR(levelAt(at(1, 18, 59, 59.9)), 80, 0.01);
  NEAR(levelAt(at(1, 19, 0)), 0, 0);
  NEAR(levelAt(at(2, 18, 30)), 0, 0);          // Tuesday: not selected
}
TEST(steady_fade_in_and_out) {
  setEvents({steady(MON, 18, 0, 60, 80, true, true, 10)});
  NEAR(levelAt(at(1, 18, 0, 5)), 80 * 0.25, 0.05);   // half way -> 25% (squared curve)
  NEAR(levelAt(at(1, 18, 0, 10)), 80, 0.05);
  NEAR(levelAt(at(1, 18, 59, 55)), 80 * 0.25, 0.05);
  NEAR(levelAt(at(1, 18, 0, 0.5)), 80 * 0.0025, 0.01); // smooth below 1 s
}
TEST(steady_fades_longer_than_event_are_shortened) {
  setEvents({steady(MON, 18, 0, 1, 100, true, true, 3600)});
  NEAR(levelAt(at(1, 18, 0, 30)), 100, 0.5);          // peak in the middle
  NEAR(levelAt(at(1, 18, 0, 15)), 25, 0.5);
}
TEST(event_crossing_midnight) {
  setEvents({steady(SAT, 23, 0, 120, 60)});
  NEAR(levelAt(at(6, 23, 30)), 60, 0.01);      // Saturday night
  NEAR(levelAt(at(0, 0, 30)), 60, 0.01);       // continues into Sunday
  NEAR(levelAt(at(0, 1, 0)), 0, 0);
  NEAR(levelAt(at(6, 0, 30)), 0, 0);           // Saturday early morning: Friday not selected
}
TEST(full_day_event) {
  setEvents({steady(ALL, 6, 0, 1440, 40)});
  NEAR(levelAt(at(3, 5, 59, 59)), 40, 0.01);
  NEAR(levelAt(at(3, 6, 0)), 40, 0.01);
}
TEST(disabled_or_no_days_is_off) {
  Event a = steady(ALL, 0, 0, 1440, 90); a.enabled = 0;
  Event b = steady(0, 0, 0, 1440, 90);
  setEvents({a, b});
  NEAR(levelAt(at(2, 12, 0)), 0, 0);
}
TEST(overlap_takes_highest) {
  setEvents({steady(MON, 18, 0, 60, 30), steady(MON, 18, 30, 60, 70)});
  NEAR(levelAt(at(1, 18, 15)), 30, 0.01);
  NEAR(levelAt(at(1, 18, 45)), 70, 0.01);
  NEAR(levelAt(at(1, 19, 15)), 70, 0.01);
}
TEST(no_clock_means_off) {
  setEvents({steady(ALL, 0, 0, 1440, 100)});
  NEAR(levelAt(0), 0, 0);
  NEAR(levelAt(1000), 0, 0);
}

// ================= schedule: interval =================
TEST(interval_user_example_60_0_60_60) {
  setEvents({interval(MON, 8, 0, 240, 100, 60, 0, 60, 60)});
  NEAR(levelAt(at(1, 8, 0, 0)), 0, 0.01);
  NEAR(levelAt(at(1, 8, 0, 30)), 25, 0.1);
  NEAR(levelAt(at(1, 8, 0, 59.99)), 100, 0.1);
  NEAR(levelAt(at(1, 8, 1, 30)), 25, 0.1);       // fading down
  NEAR(levelAt(at(1, 8, 2, 30)), 0, 0);          // fully off
  NEAR(levelAt(at(1, 8, 3, 30)), 25, 0.1);       // next cycle
  NEAR(levelAt(at(1, 11, 59, 30)), 0, 0);        // last minute of cycle 80 is "off"
  NEAR(levelAt(at(1, 12, 0, 30)), 0, 0);         // event over
}
TEST(interval_hold_only_is_constant) {
  setEvents({interval(MON, 8, 0, 10, 50, 0, 30, 0, 0)});
  NEAR(levelAt(at(1, 8, 4, 17.3)), 50, 0.01);
}
TEST(interval_instant_blink) {
  setEvents({interval(MON, 8, 0, 10, 100, 0, 5, 0, 5)});
  NEAR(levelAt(at(1, 8, 0, 2)), 100, 0.01);
  NEAR(levelAt(at(1, 8, 0, 7)), 0, 0);
  NEAR(levelAt(at(1, 8, 0, 12)), 100, 0.01);
}
TEST(interval_across_midnight) {
  setEvents({interval(SAT, 23, 59, 3, 100, 0, 30, 0, 30)});
  // cycle = 30 s on, 30 s off, starting Sat 23:59:00
  NEAR(levelAt(at(0, 0, 0, 10)), 100, 0.01);    // 70 s in  -> on
  NEAR(levelAt(at(0, 0, 0, 40)), 0, 0);         // 100 s in -> off
  NEAR(levelAt(at(0, 0, 2, 10)), 0, 0);         // 3 min event is over
}

// ================= output limits =================
TEST(output_modes_and_limits) {
  setEvents({steady(ALL, 0, 0, 1440, 90)});
  g_now = at(2, 12, 0);
  mode = MODE_AUTO; maxLevel = 100; NEAR(computeOutput(), 90, 0.01);
  maxLevel = 50; NEAR(computeOutput(), 50, 0.01);
  mode = MODE_ON; manualLevel = 70; maxLevel = 100; NEAR(computeOutput(), 70, 0.01);
  mode = MODE_OFF; NEAR(computeOutput(), 0, 0);
  mode = MODE_ON; sensorCount = 1; thermalCap = 40; sensorFault = false; NEAR(computeOutput(), 40, 0.01);
  sensorFault = true; thermalCap = 100; NEAR(computeOutput(), 30, 0.01);
  sensorCount = 0; sensorFault = false; thermalCap = 100;
}
TEST(pwm_duty_and_invert) {
  pwmInvert = false;
  pwmWrite(0); CHECK(g_duty == 0);
  pwmWrite(100); CHECK(g_duty == 4095);
  pwmWrite(50); CHECK(g_duty == 2048);
  pwmWrite(150); CHECK(g_duty == 4095);
  pwmInvert = true;
  pwmWrite(100); CHECK(g_duty == 0);
  pwmWrite(0); CHECK(g_duty == 4095);
  pwmInvert = false;
}

// ================= web: events =================
TEST(events_missing_field_does_not_wipe) {
  setEvents({steady(MON, 18, 0, 60, 80)});
  server.reset();
  handleEvents();
  CHECK(server.code == 400);
  CHECK(eventCount == 1);
}
TEST(events_empty_clears_all) {
  setEvents({steady(MON, 18, 0, 60, 80)});
  server.reset(); server.args["e"] = "";
  handleEvents();
  CHECK(server.code == 200);
  CHECK(eventCount == 0);
}
TEST(events_parse_full_and_legacy) {
  server.reset();
  server.args["e"] = "2,1080,60,80,1,0,15,1,1,60,0,60,60;127,420,30,100,0,0,5,0";
  handleEvents();
  CHECK(eventCount == 2);
  CHECK(events[0].days == 2 && events[0].start == 1080 && events[0].duration == 60 && events[0].level == 80);
  CHECK(events[0].fadeIn == 1 && events[0].fadeOut == 0 && events[0].fadeSec == 15 && events[0].enabled == 1);
  CHECK(events[0].pattern == 1 && events[0].upSec == 60 && events[0].holdSec == 0 && events[0].downSec == 60 && events[0].offSec == 60);
  CHECK(events[1].pattern == 0 && events[1].enabled == 0);   // 8-field (older page) still works
}
TEST(events_values_are_clamped) {
  server.reset();
  server.args["e"] = "255,5000,0,0,7,7,0,9,1,-5,99999,3601,0;1,-3,99999,500,0,0,99999,1,1,0,0,0,0";
  handleEvents();
  CHECK(eventCount == 2);
  CHECK(events[0].days == 127 && events[0].start == 1439 && events[0].duration == 1 && events[0].level == 1);
  CHECK(events[0].fadeIn == 1 && events[0].fadeOut == 1 && events[0].fadeSec == 1 && events[0].enabled == 1);
  CHECK(events[0].upSec == 0 && events[0].holdSec == 3600 && events[0].downSec == 3600 && events[0].offSec == 0);
  CHECK(events[1].start == 0 && events[1].duration == 1440 && events[1].level == 100 && events[1].fadeSec == 3600);
  CHECK(events[1].pattern == 0);                             // interval with all-zero times -> steady
}
TEST(events_garbage_is_skipped_and_max_20) {
  server.reset();
  std::string s = "abc;1,2,3;;";
  for (int i = 0; i < 25; i++) s += "2,600,10,50,0,0,5,1;";
  server.args["e"] = s;
  handleEvents();
  CHECK(eventCount == MAX_EVENTS);
}
TEST(events_survive_reboot) {
  resetAll();
  server.args["e"] = "2,1080,60,80,1,0,15,1,1,60,0,60,60";
  handleEvents();
  eventCount = 0; memset(events, 0, sizeof(events));
  loadSettings();
  CHECK(eventCount == 1 && events[0].start == 1080 && events[0].pattern == 1 && events[0].offSec == 60);
}

// ================= storage migration =================
TEST(migrate_from_v2_fade_seconds) {
  resetAll();
  EventV2 old[2] = {{2, 80, 1080, 60, 1, 0, 15, 1}, {127, 40, 420, 30, 0, 1, 90, 0}};
  g_nvs["uvtimer2/events2"] = std::vector<uint8_t>((uint8_t *)old, (uint8_t *)old + sizeof old);
  loadSettings();
  CHECK(eventCount == 2);
  CHECK(events[0].days == 2 && events[0].level == 80 && events[0].start == 1080 && events[0].fadeSec == 15 && events[0].pattern == 0);
  CHECK(events[1].fadeOut == 1 && events[1].fadeSec == 90 && events[1].enabled == 0);
  CHECK(g_nvs.count("uvtimer2/events3"));                    // converted copy saved
}
TEST(migrate_from_v1_fade_minutes) {
  resetAll();
  EventV1 old[1] = {{2, 80, 1080, 60, 1, 1, 2, 1}};
  g_nvs["uvtimer2/events"] = std::vector<uint8_t>((uint8_t *)old, (uint8_t *)old + sizeof old);
  loadSettings();
  CHECK(eventCount == 1 && events[0].fadeSec == 120 && events[0].pattern == 0);
}
TEST(fresh_device_defaults) {
  resetAll();
  loadSettings();
  CHECK(eventCount == 0 && mode == MODE_AUTO && maxLevel == 100 && pwmFreq == 1000);
  CHECK(apSsid == "UV-Timer" && apPass == "uvtimer123");
}

// ================= clock =================
TEST(set_time_validation) {
  resetAll();
  server.args["t"] = "123"; handleTime(); CHECK(server.code == 400);
  server.reset(); server.args["t"] = "abc"; handleTime(); CHECK(server.code == 400);
  server.reset(); server.args["t"] = "9999999999"; handleTime(); CHECK(server.code == 400);
  server.reset(); server.args["t"] = "1793000000"; handleTime(); CHECK(server.code == 200);
  CHECK((long)g_now == 1793000000L);
  CHECK(timeSet());
}
TEST(ds3231_keeps_time_across_power_cut) {
  resetAll();
  Wire.present = true; Wire.reg[0x0F] = 0x80;               // new module: "oscillator stopped"
  rtcBegin();
  CHECK(rtcFound);
  CHECK(!timeSet());                                         // module time not trusted yet
  double t = at(4, 21, 37, 12);
  server.args["t"] = std::to_string((long)t); handleTime();
  CHECK(server.code == 200);
  CHECK((Wire.reg[0x0F] & 0x80) == 0);                       // flag cleared
  g_now = 0;                                                 // power cut
  rtcBegin();
  NEAR(g_now, t, 0.5);
  struct tm tmv; time_t tt = (time_t)g_now; gmtime_r(&tt, &tmv);
  CHECK(tmv.tm_wday == 4 && tmv.tm_hour == 21 && tmv.tm_min == 37 && tmv.tm_sec == 12);
}
TEST(no_ds3231_is_fine) {
  resetAll();
  rtcBegin();
  CHECK(!rtcFound && !timeSet());
}

// ================= temperature =================
static uint32_t mvFor(double c) { double r = 100000 * exp(3950 * (1 / (c + 273.15) - 1 / 298.15)); return (uint32_t)lround(3300 * r / (r + 100000)); }
TEST(ntc_conversion) {
  g_mv[3] = mvFor(25); NEAR(readNtc(3), 25, 0.5);
  g_mv[3] = mvFor(50); NEAR(readNtc(3), 50, 0.5);
  g_mv[3] = mvFor(80); NEAR(readNtc(3), 80, 1.0);
  g_mv[3] = 0; CHECK(isnan(readNtc(3)));                      // short
  g_mv[3] = 3300; CHECK(isnan(readNtc(3)));                   // disconnected
}
TEST(thermal_limit_controller) {
  resetAll();
  sensorCount = 1; tempLimit = 50;
  g_mv[3] = mvFor(55);
  for (int i = 0; i < 5; i++) updateThermal();
  NEAR(thermalCap, 95, 0.01);
  for (int i = 0; i < 100; i++) updateThermal();
  NEAR(thermalCap, 0, 0.01);
  g_mv[3] = mvFor(40);
  for (int i = 0; i < 25; i++) updateThermal();
  NEAR(thermalCap, 10, 0.01);                                 // recovers slowly
  g_mv[3] = mvFor(70);                                        // 20 C over -> emergency
  updateThermal(); NEAR(thermalCap, 0, 0.01);
  g_mv[3] = 3300; updateThermal();                            // unplugged
  CHECK(sensorFault); CHECK(thermalLimit() <= 30);
  sensorCount = 0; updateThermal(); NEAR(thermalLimit(), 100, 0);
}

// ================= other handlers =================
TEST(mode_and_settings_clamp) {
  resetAll();
  server.args["mode"] = "7"; server.args["level"] = "300"; handleMode();
  CHECK(mode == MODE_OFF && manualLevel == 100);
  server.reset(); server.args["freq"] = "50"; server.args["tlimit"] = "5"; server.args["maxLevel"] = "0";
  server.args["sensors"] = "9"; server.args["invert"] = "1"; handleSettings();
  CHECK(pwmFreq == 100 && g_freq == 100 && tempLimit == 30 && maxLevel == 1 && sensorCount == 2 && pwmInvert);
  server.reset(); server.args["freq"] = "999999"; handleSettings(); CHECK(pwmFreq == 19000);
  loadSettings();                                             // all persisted
  CHECK(mode == MODE_OFF && pwmFreq == 19000 && tempLimit == 30 && maxLevel == 1 && sensorCount == 2 && pwmInvert);
}
TEST(wifi_validation) {
  resetAll();
  server.args["ssid"] = "Lab"; server.args["pass"] = "short"; handleWifi();
  CHECK(server.code == 400 && ESP.restarts == 0);
  server.reset(); server.args["ssid"] = std::string(33, 'x'); server.args["pass"] = ""; handleWifi();
  CHECK(server.code == 400);
  server.reset(); server.args["ssid"] = "   "; handleWifi(); CHECK(server.code == 400);
  server.reset(); server.args["ssid"] = " Lab UV "; server.args["pass"] = "12345678"; handleWifi();
  CHECK(server.code == 200 && ESP.restarts == 1);
  loadSettings(); CHECK(apSsid == "Lab UV" && apPass == "12345678");
  server.reset(); server.args["ssid"] = "Open"; server.args["pass"] = ""; handleWifi();
  loadSettings(); setup(); CHECK(WiFi.ssid == "Open" && WiFi.open);
}

// ================= JSON + full boot =================
TEST(state_json_written_for_validation) {
  resetAll();
  apSsid = "My \"UV\" \\ net";
  sensorCount = 2; temps[0] = 41.25f; temps[1] = NAN;
  setEvents({steady(MON, 18, 0, 60, 80, true, false, 15), interval(ALL, 8, 0, 240, 100, 60, 0, 60, 60)});
  g_now = at(1, 18, 30); output = 80;
  handleState();
  CHECK(server.code == 200);
  FILE *f = fopen("state.json", "w"); fputs(server.body.c_str(), f); fclose(f);
}
TEST(boot_and_loop_smoke) {
  resetAll();
  setup();
  CHECK(g_duty == 0);                                         // LED off at boot
  CHECK(WiFi.ssid == "UV-Timer" && !WiFi.open);
  server.args["e"] = "127,0,1440,60,0,0,5,1"; handleEvents();
  server.reset(); server.args["t"] = std::to_string((long)at(2, 10, 0)); handleTime();
  for (int i = 0; i < 40; i++) { g_millis += 50; loop(); }
  NEAR(output, 60, 0.01);
  CHECK(g_duty == (uint32_t)(60 * 4095 / 100.0 + 0.5));
  CHECK(g_pin[STATUS_LED_PIN] == LOW);                        // blue LED shows "on"
}

int main() {
  setenv("TZ", "UTC0", 1); tzset();
  for (auto &t : tests()) { int before = g_fail; t.second(); printf("%s %s\n", g_fail == before ? "ok  " : "FAIL", t.first); }
  printf("\n%d checks passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
