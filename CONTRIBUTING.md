# Contributing / maintaining this repository

## Build and test

```bash
tools/build_all.sh            # all 17 STM32 + 15 8051 projects (needs arm-none-eabi-gcc and sdcc)
tools/build_all.sh stm32      # only STM32
tools/build_all.sh 8051       # only 8051
make -C tests                 # host-side tests (MQTT packet parser, ESPLib, ESP8266 AT driver)
```

- macOS: `brew install sdcc`; the ARM GCC bundled with STM32CubeIDE is found automatically.
- Ubuntu: `sudo apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi sdcc sdcc-libraries`.
- GitHub Actions runs the same commands on every push (`.github/workflows/build.yml`).

The STM32 projects themselves are normal STM32CubeIDE projects (File → Import → Existing Projects
into Workspace); the script only reproduces the build from the command line.

## Shared libraries are duplicated — keep the copies in sync

STM32CubeIDE compiles everything inside a project's `Core/` folder, so each project carries its own
copy of the libraries it uses. `Common_Libraries/` holds the master copies.

| Master copy | Copies to update |
|---|---|
| `Common_Libraries/ESPLib_MQTT/` (`ESPLib.*`, `mqtt_packet.*`) | `STM32/CAT13_*/CAT13A_*`, `CAT13B_*`, `CAT13C_*` — `Core/Src` + `Core/Inc` |
| `Common_Libraries/ESP8266_AT_Driver/` (`esp_at.*`) | `STM32/CAT10_*/CAT10D_*`, `STM32/CAT12_*/CAT12B_*` |
| `Common_Libraries/BME280_Bosch_API/` | `STM32/CAT12_*/CAT12A_*`, `CAT12B_*`, `STM32/CAT13_*/CAT13C_*` |
| `Common_Libraries/Helpers/` (`wifi_config.h`, `fmt_fixed.h`) | the Wi-Fi / BME280 projects above |
| 8051 `delay.c/.h`, `lcd.c/.h` | each 8051 task folder that uses them |

Workflow: change the master copy → run `make -C tests` → copy it into the projects → run
`tools/build_all.sh`. For example:

```bash
L=Common_Libraries/ESPLib_MQTT
for p in STM32/CAT13*/CAT13*/; do cp $L/*.c "$p/Core/Src/"; cp $L/*.h "$p/Core/Inc/"; done
```

## 8051 HEX files

`Objects/*.hex` are built with SDCC from the Keil sources. After changing 8051 code run
`UPDATE_HEX=1 tools/build_all.sh 8051` so the committed HEX files match the sources again
(the normal build reports "matches committed HEX" for every project).

## Wi-Fi credentials

Never commit them. Put them in `Core/Inc/wifi_secrets.h` (git-ignored); `wifi_config.h` explains the format.

## Build log

`docs/build-log.txt` is the output of `tools/build_all.sh` followed by `make -C tests`. Regenerate it
after significant changes.
