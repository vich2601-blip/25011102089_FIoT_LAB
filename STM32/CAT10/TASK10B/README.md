# EXPERIMENT DETAILS

**EXP:** CONTINUOUS ASSESSMENT 10

**DATE:** 05/10/2026

**NAME OF EXP:** STM32–Wemos Wi-Fi Modem Interface Using AT Commands

**HARDWARE SETUP / PLATFORM:** Blue Pill STM32F103C8T6 (Cortex-M3, 72 MHz) + Wemos D1 mini (ESP8266 AT firmware) + USB-to-TTL — STM32CubeMX + Keil MDK-ARM (STM32CubeIDE project also included), STM32 HAL, PuTTY

---

## TASK 9B : Explore the ESP8266 AT Command Interface

> The CAT 10 lab sheet numbers this task **9B**; it is stored in folder `TASK10B`.

### 1. Circuit Diagram

| STM32 | Connected to | Purpose |
|---|---|---|
| PA9 USART1_TX | Wemos RX | AT commands |
| PA10 USART1_RX | Wemos TX | responses |
| PA2 USART2_TX / PA3 USART2_RX | USB-to-TTL RX / TX | PC terminal |
| GND | all GNDs | common ground |

Both UARTs 115200-8-N-1. Wi-Fi credentials go in `Core/Inc/wifi_secrets.h` (git-ignored; see `wifi_config.h` for the format).

![Circuit Diagram](circuit.png)

*Drawn on [Wokwi](https://wokwi.com). To open and edit it: start a new project on wokwi.com, paste [`wokwi/diagram.json`](wokwi/diagram.json) into its diagram.json tab, and add the `wokwi/*.chip.json` files (if any) with **New file**. The AT89S52, USB-TTL, ULN2003, USBasp and ST-LINK boards are custom chips (pin names added as labels). The ESP-01 part stands in for the Wemos D1 mini and the BMP180 module for the BME280 (same AT firmware / same 4-pin I²C header); the Nucleo-64 graphic is labelled as the F401RE.*

### 2. Core Logic / Pseudocode / Code Base

**Working Principle:**

Every command is a CR LF-terminated line and every reply ends with a status line.

| Requirement | Command |
|---|---|
| Modem response | `AT` |
| Firmware | `AT+GMR` |
| Station mode | `AT+CWMODE=1` |
| Scan | `AT+CWLAP` |
| Join AP | `AT+CWJAP="ssid","pwd"` |
| Network info | `AT+CIFSR` |
| TCP connection | `AT+CIPSTART="TCP","host",port` |
| Send data | `AT+CIPSEND=<len>` |

**Pseudocode:**

1. Startup as 10A.
2. For AT, AT+GMR, AT+CWMODE=1, AT+CWLAP: print, send, collect, print replies.
3. Print the requirement table.
4. Bridge mode.

**Code Base:**

- Project files: `CAT10B_ESP8266_AT_Command_Explorer.ioc` (CubeMX configuration), `MDK-ARM/CAT10B_ESP8266_AT_Command_Explorer.uvprojx` (Keil MDK), `.project` / `.cproject` (STM32CubeIDE); application code in `Core/Src/main.c`.
- Replies are buffered (1 KB) before printing so no ESP byte is lost while PuTTY is written

**Expected Output:**

```
>> AT+GMR
<< AT version:...
<< SDK version:...
<< OK
>> AT+CWLAP
<< +CWLAP:(3,"SNUC",-58,...)
<< OK
```

### 3. Inference

One request/response routine is enough to check the modem, read its firmware, set station mode and scan networks.
