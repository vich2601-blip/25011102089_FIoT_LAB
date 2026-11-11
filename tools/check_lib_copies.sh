#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# check_lib_copies.sh - every STM32 project carries its own copy of the shared
# libraries (CubeIDE only compiles files inside the project). This script checks
# that each copy is byte-identical to the master in Common_Libraries/, so the
# host tests (which compile the master) really test what the firmware runs.
#
# Usage: tools/check_lib_copies.sh          -> report, exit 1 on any mismatch
#        tools/check_lib_copies.sh --fix    -> copy the masters into the projects
# ---------------------------------------------------------------------------
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1
FIX=0; [ "${1:-}" = "--fix" ] && FIX=1
bad=0; checked=0

# master file : project glob : sub-folder inside the project
MAP=(
  "Common_Libraries/ESPLib_MQTT/ESPLib.c|STM32/CAT13/TASK13*|Core/Src"
  "Common_Libraries/ESPLib_MQTT/mqtt_packet.c|STM32/CAT13/TASK13*|Core/Src"
  "Common_Libraries/ESPLib_MQTT/ESPLib.h|STM32/CAT13/TASK13*|Core/Inc"
  "Common_Libraries/ESPLib_MQTT/mqtt_packet.h|STM32/CAT13/TASK13*|Core/Inc"
  "Common_Libraries/ESP8266_AT_Driver/esp_at.c|STM32/CAT10/TASK10D STM32/CAT12/TASK12B|Core/Src"
  "Common_Libraries/ESP8266_AT_Driver/esp_at.h|STM32/CAT10/TASK10D STM32/CAT12/TASK12B|Core/Inc"
  "Common_Libraries/BME280_Bosch_API/bme280.c|STM32/CAT12/TASK12A STM32/CAT12/TASK12B STM32/CAT13/TASK13C|Core/Src"
  "Common_Libraries/BME280_Bosch_API/bme280.h|STM32/CAT12/TASK12A STM32/CAT12/TASK12B STM32/CAT13/TASK13C|Core/Inc"
  "Common_Libraries/BME280_Bosch_API/bme280_defs.h|STM32/CAT12/TASK12A STM32/CAT12/TASK12B STM32/CAT13/TASK13C|Core/Inc"
  "Common_Libraries/Helpers/wifi_config.h|STM32/CAT10/TASK10C STM32/CAT10/TASK10D STM32/CAT12/TASK12B STM32/CAT13/TASK13*|Core/Inc"
  "Common_Libraries/Helpers/fmt_fixed.h|STM32/CAT12/TASK12A STM32/CAT12/TASK12B STM32/CAT13/TASK13C|Core/Inc"
)

for entry in "${MAP[@]}"; do
  IFS='|' read -r master globs sub <<< "$entry"
  for proj in $globs; do
    [ -d "$proj" ] || continue
    copy="$proj/$sub/$(basename "$master")"
    checked=$((checked + 1))
    if ! cmp -s "$master" "$copy"; then
      if [ $FIX = 1 ]; then cp "$master" "$copy"; echo "fixed     $copy"
      else echo "MISMATCH  $copy  (master: $master)"; bad=$((bad + 1)); fi
    fi
  done
done

echo "checked $checked library copies, $bad mismatched"
[ $bad -eq 0 ]
