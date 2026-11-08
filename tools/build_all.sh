#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# build_all.sh - compile every project in this repository from the command line
#
#   STM32 : every STM32CubeIDE project (folder with a .cproject) is compiled and
#           linked with arm-none-eabi-gcc, using the same flags as CubeIDE.
#   8051  : every Keil project (folder with main.c under 8051/) is compiled with
#           SDCC. Keil C51 keywords are mapped to SDCC syntax on a temporary copy;
#           the sources in the repository are not modified.
#
# Requirements: arm-none-eabi-gcc (+ newlib) and sdcc on PATH.
#   macOS : brew install sdcc   (arm gcc is bundled with STM32CubeIDE and found automatically)
#   Ubuntu: sudo apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi sdcc
#
# Usage:  tools/build_all.sh [stm32|8051|all]      (default: all)
#         UPDATE_HEX=1 tools/build_all.sh 8051   -> also refresh the committed Objects/*.hex
# Exit status is non-zero if any project fails to build.
# ---------------------------------------------------------------------------
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WHAT="${1:-all}"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
fail=0; pass=0

# ------------------------------------------------------------------ tools
find_arm_gcc() {
  if [ -n "${ARM_GCC:-}" ]; then echo "$ARM_GCC"; return; fi
  if command -v arm-none-eabi-gcc >/dev/null 2>&1; then command -v arm-none-eabi-gcc; return; fi
  find /Applications/STM32CubeIDE.app -name arm-none-eabi-gcc -type f 2>/dev/null | head -1
}

# ------------------------------------------------------------------ STM32
build_stm32() {
  local CC; CC="$(find_arm_gcc)"
  if [ -z "$CC" ]; then echo "arm-none-eabi-gcc not found - skipping STM32"; fail=$((fail + 1)); return; fi
  local SIZE="${CC%gcc}size"
  echo "== STM32 ($("$CC" --version | head -1))"
  while IFS= read -r cproject; do
    local P; P="$(dirname "$cproject")"
    local name; name="$(basename "$P")"
    local dev; dev="$(grep -o 'STM32F[0-9]\{3\}x[0-9A-Z]' "$cproject" | head -1)"
    local fam cpu
    case "$dev" in
      STM32F1*) fam=STM32F1xx; cpu=(-mcpu=cortex-m3 -mthumb -mfloat-abi=soft) ;;
      STM32F4*) fam=STM32F4xx; cpu=(-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard) ;;
      *) echo "  ?? $name: unknown device"; fail=$((fail + 1)); continue ;;
    esac
    local inc=(-I"$P/Core/Inc" -I"$P/Drivers/${fam}_HAL_Driver/Inc" -I"$P/Drivers/${fam}_HAL_Driver/Inc/Legacy"
               -I"$P/Drivers/CMSIS/Device/ST/$fam/Include" -I"$P/Drivers/CMSIS/Include")
    local flags=("${cpu[@]}" -std=gnu11 -O0 -g -DUSE_HAL_DRIVER -D"$dev" "${inc[@]}" -ffunction-sections -fdata-sections)
    local o="$OUT/stm32/$name"; mkdir -p "$o"
    local log="$o/build.log"; : > "$log"; local ok=1
    for f in "$P"/Core/Src/*.c; do
      "$CC" "${flags[@]}" -Wall -Wextra -Wno-unused-parameter -c "$f" -o "$o/$(basename "$f" .c).o" 2>>"$log" || ok=0
    done
    for f in "$P"/Drivers/"${fam}"_HAL_Driver/Src/*.c; do
      "$CC" "${flags[@]}" -w -c "$f" -o "$o/hal_$(basename "$f" .c).o" 2>>"$log" || ok=0
    done
    "$CC" "${cpu[@]}" -x assembler-with-cpp -c "$P"/Core/Startup/*.s -o "$o/startup.o" 2>>"$log" || ok=0
    "$CC" "${cpu[@]}" -T"$(ls "$P"/*FLASH.ld | head -1)" --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections \
          "$o"/*.o -o "$o/firmware.elf" -lc -lm 2>>"$log" || ok=0
    local warns; warns="$(grep -c 'warning:' "$log" | tr -d ' ')"
    if [ $ok = 1 ]; then
      local sz; sz="$("$SIZE" "$o/firmware.elf" | awk 'NR==2 {printf "flash %6d B  ram %5d B", $1 + $2, $2 + $3}')"
      printf "  OK    %-42s %-12s %s  warnings: %s\n" "$name" "$dev" "$sz" "$warns"; pass=$((pass + 1))
    else
      printf "  FAIL  %-42s %s\n" "$name" "$dev"; grep -E 'error|warning' "$log" | head -5 | sed 's/^/        /'; fail=$((fail + 1))
    fi
  done < <(find "$ROOT/STM32" -name .cproject | sort)
}

# ------------------------------------------------------------------ 8051
build_8051() {
  if ! command -v sdcc >/dev/null 2>&1; then echo "sdcc not found - skipping 8051"; fail=$((fail + 1)); return; fi
  echo "== 8051 ($(sdcc --version | head -1 | cut -c1-60))"
  while IFS= read -r main; do
    local d; d="$(dirname "$main")"; local name; name="$(basename "$d")"
    local o="$OUT/8051/$name"; mkdir -p "$o"
    # Keil C51 -> SDCC keyword mapping on a temporary copy
    for f in "$d"/*.c "$d"/*.h; do
      [ -e "$f" ] || continue
      perl -pe 's/#include <reg52\.h>/#include <8052.h>/;
                s/^sbit +(\w+) *= *P([0-3])\^([0-7]);/#define $1 P$2_$3/;
                s/\bbit\b/__bit/g;
                s/interrupt (\d+) using (\d+)/__interrupt($1) __using($2)/;
                s/\bidata\b/__idata/g;
                s/^unsigned (char|int)\s+code\s+\*\s*code\s+/const unsigned $1 __code * const __code /;
                s/^unsigned (char|int)\s+code\s+\*/const unsigned $1 __code */;
                s/^unsigned (char|int)\s+code\s+/const unsigned $1 __code /' "$f" > "$o/$(basename "$f")"
    done
    # SDCC needs ISR prototypes visible in the file that contains main()
    [ -f "$o/uart.h" ] && perl -0pi -e 's/#endif\s*$/void uart_isr(void) __interrupt(4) __using(2);\n\n#endif\n/' "$o/uart.h"
    local ok=1
    ( cd "$o" && for c in *.c; do sdcc -mmcs51 --model-small -c "$c" 2>>build.log || exit 1; done
      others=$(ls *.rel | grep -v '^main.rel$' || true)
      sdcc -mmcs51 --model-small --code-size 8192 -o "$name.ihx" main.rel $others 2>>build.log || exit 1
      packihx "$name.ihx" > "$name.hex" 2>/dev/null ) || ok=0
    if [ $ok = 1 ]; then
      if [ "${UPDATE_HEX:-0}" = 1 ]; then mkdir -p "$d/Objects"; cp "$o/$name.hex" "$d/Objects/$name.hex"; fi
      local match="new"
      if [ -f "$d/Objects/$name.hex" ]; then cmp -s "$o/$name.hex" "$d/Objects/$name.hex" && match="matches committed HEX" || match="differs from committed HEX (other SDCC version?)"; fi
      printf "  OK    %-42s %s\n" "$name" "$match"; pass=$((pass + 1))
    else
      printf "  FAIL  %s\n" "$name"; head -5 "$o/build.log" | sed 's/^/        /'; fail=$((fail + 1))
    fi
  done < <(find "$ROOT/8051" -name main.c | sort)
}

case "$WHAT" in
  stm32) build_stm32 ;;
  8051)  build_8051 ;;
  *)     build_stm32; build_8051 ;;
esac
echo
echo "Result: $pass built, $fail failed"
[ $fail -eq 0 ]
