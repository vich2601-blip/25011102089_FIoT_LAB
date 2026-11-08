# Common Libraries

Reference copies of the shared libraries. Each STM32 project carries its own copy in `Core/Src` + `Core/Inc`
(CubeIDE builds everything in those folders), so these are the master versions.

| Folder | Files | Used in | Origin / version | Licence | Limitations |
|---|---|---|---|---|---|
| `BME280_Bosch_API` | `bme280.c/.h`, `bme280_defs.h` | CAT 12, 13 | Bosch Sensortec BME280_SensorAPI v3.5.1, unchanged | BSD-3-Clause | needs the application to supply `read`, `write`, `delay_us`; built with `BME280_DOUBLE_ENABLE` (software doubles on the M4) |
| `ESPLib_MQTT` | `ESPLib.c/.h`, `mqtt_packet.c/.h` | CAT 13 | new implementation of the course ESPLib API (function names kept for the lab sheet) | MIT | single TCP connection (CIPMUX=0); QoS 0 only (QoS 1/2 requests return an error, incoming QoS 1/2 messages are dropped); incoming packets > 256 B, PUBLISH payloads > 127 B and topics > 63 characters are dropped and counted; one pending received message at a time; plaintext MQTT (port 1883) |
| `ESP8266_AT_Driver` | `esp_at.c/.h` | CAT 10D, 12B | original | MIT | blocking `AT+CIPSEND` exchange (bounded by timeouts); lines longer than 127 characters are cut |
| `Helpers` | `fmt_fixed.h`, `wifi_config.h` | CAT 10C – 13 | original | MIT | `fmt_2dp()` handles values within ±21 474 836.47 (int32 × 100); credentials go in the git-ignored `wifi_secrets.h` |

On the 8051 side, `delay.c/.h` (calibrated busy-wait with a volatile counter), `uart.c/.h` (CAT 6C, interrupt-driven, 16-byte RX ring buffer) and `lcd.c/.h` (CAT 7, HD44780 4-bit) live inside the experiment folders.

Tests: `make -C tests` runs the MQTT packet tests and the ESPLib tests against a simulated ESP8266 on the PC.
