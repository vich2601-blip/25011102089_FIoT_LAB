# EXPERIMENT DETAILS

**EXP:** CONTINUOUS ASSESSMENT 8

**DATE:** 18/09/2026

**NAME OF EXP:** STM32 GPIO Bring-Up and Switch-Controlled LED

**HARDWARE SETUP / PLATFORM:** Blue Pill STM32F103C6T6 (Cortex-M3, 72 MHz) + push-button — STM32CubeMX + Keil MDK-ARM (STM32CubeIDE project also included), STM32 HAL

---

## TASK 8C : Short Press vs Long Press

### 1. Circuit Diagram

| Pin | CubeMX setting | Connected to |
|---|---|---|
| PC13 | GPIO_Output, push-pull | on-board LED (active-LOW) |
| PA15 | GPIO_Input, pull-up | push-button → GND |
| PA13 / PA14 | SYS: Debug = Serial Wire | ST-LINK (also frees PA15 from JTAG) |
| RCC | HSE crystal → PLL ×9 | 72 MHz |

![Circuit Diagram](circuit.png)

*Drawn on [Wokwi](https://wokwi.com). To open and edit it: start a new project on wokwi.com, paste [`wokwi/diagram.json`](wokwi/diagram.json) into its diagram.json tab, and add the `wokwi/*.chip.json` files (if any) with **New file**. The AT89S52, USB-TTL, ULN2003, USBasp and ST-LINK boards are custom chips (pin names added as labels). The ESP-01 part stands in for the Wemos D1 mini and the BMP180 module for the BME280 (same AT firmware / same 4-pin I²C header); the Nucleo-64 graphic is labelled as the F401RE.*

### 2. Core Logic / Pseudocode / Code Base

**Working Principle:**

Press edge → record start time → keep running → release edge → duration = release − start → classify. < 1 s is short, ≥ 1 s is long. The press is classified **when the button is released**, so blinking starts on release of a long press, not after 1 s of holding.

| Action | Response |
|---|---|
| Short | toggle LED |
| Long | continuous blinking (200 ms) |
| Short while blinking | stop blinking |
| Long again | blink again |

**Pseudocode:**

1. On a debounced press edge store `press_start_ms`.
2. On release compute the duration.
3. Long → `blinking = 1`.
4. Short → stop blinking if active, otherwise toggle the LED.
5. If blinking and 200 ms passed, toggle and store the time.

**Code Base:**

- Project files: `CAT08C_Short_vs_Long_Press.ioc` (CubeMX configuration), `MDK-ARM/CAT08C_Short_vs_Long_Press.uvprojx` (Keil MDK), `.project` / `.cproject` (STM32CubeIDE); application code in `Core/Src/main.c`.
- `classify_press()`
- `HAL_GetTick()` for both press timing and blink interval — no `HAL_Delay()`

### 3. Inference

Time stamps instead of blocking delays let the program classify presses while the LED keeps blinking.
