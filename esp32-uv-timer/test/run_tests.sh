#!/bin/sh
# Compiles the real firmware against host stubs and runs the unit tests.
# Builds twice: once for ESP32 Arduino core 3.x, once for core 2.x.
set -e
cd "$(dirname "$0")"
for core in 3 2; do
  echo "== ESP32 core $core.x API =="
  g++ -std=gnu++17 -O1 -g -Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined \
      -DESP_ARDUINO_VERSION_MAJOR=$core -I stubs -I ../uv_timer test_firmware.cpp -o test_bin_$core
  ./test_bin_$core
done
python3 -c "import json; d=json.load(open('state.json')); assert d['ssid']=='My \"UV\" \\\\ net' and len(d['events'])==2 and d['temps'][1] is None; print('state.json is valid JSON')"
rm -f test_bin_2 test_bin_3 state.json
