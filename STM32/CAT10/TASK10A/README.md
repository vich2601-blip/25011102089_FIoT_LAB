# EXPERIMENT DETAILS

**EXP:** CONTINUOUS ASSESSMENT 10

**DATE:** 05/10/2026

**NAME OF EXP:** STM32–Wemos Wi-Fi Modem Interface Using AT Commands

**HARDWARE SETUP / PLATFORM:** Blue Pill STM32F103C8T6 (Cortex-M3, 72 MHz) + Wemos D1 mini (ESP8266 AT firmware) + USB-to-TTL — STM32CubeMX + Keil MDK-ARM (STM32CubeIDE project also included), STM32 HAL, PuTTY

---

## TASK 9A : Establish STM32 ↔ Wemos UART Communication

> The CAT 10 lab sheet numbers this task **9A**; it is stored in folder `TASK10A`.

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

After a 2 s boot wait the STM32 sends `AT`, collects reply lines until `OK`/`ERROR`/`FAIL` (with a timeout) and prints them. It then enters a bridge mode that forwards PuTTY ↔ ESP8266 so other commands can be tried by hand.

**Pseudocode:**

1. Banner on USART2.
2. `esp_startup()`: wait, retry `AT` up to 3 times, `ATE0`.
3. `at_show("AT")` → prints `>> AT`, `<< OK`.
4. `at_bridge_start()`: both UARTs switch to interrupt-driven receive, each into its own ring buffer (256 slots, 255 usable bytes); if re-arming the receive interrupt ever fails, the main loop retries it.
5. Sending is interrupt-driven too: each direction has a 512-slot transmit ring (511 usable bytes) sent with `HAL_UART_Transmit_IT`, so forwarding never blocks the loop; a full transmit ring is counted separately from receive overflows.
6. Loop: `at_bridge_poll()` forwards everything that has arrived in batches (CR from PuTTY → CR LF to the ESP) and reports lost bytes and transmit failures, so typing and long ESP replies do not drop characters.

**Code Base:**

- Project files: `CAT10A_STM32_Wemos_UART_Link.ioc` (CubeMX configuration), `MDK-ARM/CAT10A_STM32_Wemos_UART_Link.uvprojx` (Keil MDK), `.project` / `.cproject` (STM32CubeIDE); application code in `Core/Src/main.c`.
- `esp_read_line()` — line reader with timeout
- `at_command()` — collect until status line
- `at_show()` — print command and replies

**Expected Output:**

```
STM32-WiFi MODEM TEST  (CAT10A)
ESP8266 answered, echo off
>> AT
<< OK
```

### 3. Inference

The Wemos replies OK, so the two boards communicate; a separate UART keeps debug output away from AT traffic.
