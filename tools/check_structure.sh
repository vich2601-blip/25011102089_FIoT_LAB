#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# check_structure.sh - runs the same file checks as the course evaluator on
# every task folder (a folder holding a Keil .uvproj or a CubeMX .ioc).
#   8051 : *.uvproj/*.uvprojx, *.c, STARTUP.A51, Objects/, Objects/*.hex, README.md, circuit.png
#   STM32: *.ioc, .mxproject, Core/, Drivers/, MDK-ARM/, MDK-ARM/*.uvprojx,
#          MDK-ARM/startup_*.s, README.md, circuit.png
# README sections: EXP, DATE, NAME OF EXP, HARDWARE SETUP / PLATFORM, TASK,
#          Circuit Diagram (with ![...](circuit.png)), Core Logic, Inference.
# ---------------------------------------------------------------------------
set -u
cd "$(dirname "$0")/.." || exit 1
tasks=0; passed=0; checks=0; ok=0
check() {  # name, condition-result(0/1)
  checks=$((checks + 1))
  if [ "$2" = 0 ]; then ok=$((ok + 1)); else echo "    [FAIL] $1"; fail=1; fi
}
has() { compgen -G "$1" > /dev/null; echo $?; }
readme_ok() {
  local r="$1/README.md"; [ -f "$r" ] || { echo 1; return; }
  for k in '\*\*EXP:\*\*' '\*\*DATE:\*\*' '\*\*NAME OF EXP:\*\*' '\*\*HARDWARE SETUP / PLATFORM:\*\*' '^## TASK|^## ACTIVITY|^## STEP|^## ' \
           'Circuit Diagram' '!\[[^]]*\]\(circuit\.png\)' 'Core Logic' 'Inference'; do
    grep -Eq "$k" "$r" || { echo 1; return; }
  done
  echo 0
}
while IFS= read -r d; do
  tasks=$((tasks + 1)); fail=0
  echo "$d"
  case "$d" in
    8051/*)
      check "Keil project (*.uvproj or *.uvprojx)" "$( [ "$(has "$d/*.uvproj")" = 0 ] || [ "$(has "$d/*.uvprojx")" = 0 ]; echo $?)"
      check "C source file (*.c)" "$(has "$d/*.c")"
      check "STARTUP.A51" "$( [ -f "$d/STARTUP.A51" ]; echo $?)"
      check "Objects/ folder" "$( [ -d "$d/Objects" ]; echo $?)"
      check "Objects/*.hex" "$(has "$d/Objects/*.hex")" ;;
    STM32/*)
      check ".ioc file" "$(has "$d/*.ioc")"
      check ".mxproject" "$( [ -f "$d/.mxproject" ]; echo $?)"
      check "Core/ folder" "$( [ -d "$d/Core" ]; echo $?)"
      check "Drivers/ folder" "$( [ -d "$d/Drivers" ]; echo $?)"
      check "MDK-ARM/ folder" "$( [ -d "$d/MDK-ARM" ]; echo $?)"
      check "MDK-ARM/*.uvprojx" "$(has "$d/MDK-ARM/*.uvprojx")"
      check "MDK-ARM/startup_*.s" "$(has "$d/MDK-ARM/startup_*.s")" ;;
  esac
  check "README.md exists" "$( [ -f "$d/README.md" ]; echo $?)"
  check "circuit.png exists" "$( [ -f "$d/circuit.png" ]; echo $?)"
  check "README has all required sections" "$(readme_ok "$d")"
  if [ $fail = 0 ]; then passed=$((passed + 1)); echo "    Task result: PASSED"; else echo "    Task result: FAILED"; fi
done < <(find 8051 STM32 \( -name '*.uvproj' -o -name '*.uvprojx' -o -name '*.ioc' \) -not -path '*/MDK-ARM/*' -exec dirname {} \; | sort -u -V)
echo
echo "Tasks discovered: $tasks, passed: $passed, failed: $((tasks - passed)); file checks passed: $ok/$checks"
[ $passed = $tasks ]
