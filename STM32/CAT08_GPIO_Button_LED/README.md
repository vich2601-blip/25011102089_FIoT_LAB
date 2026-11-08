# CAT 8 — STM32 GPIO Bring-Up and Switch-Controlled LED

| Field | Details |
|---|---|
| Experiment | Continuous Lab Evaluation 8 |
| Date | 18/09 (Week 7) |
| Experiment name | STM32 GPIO Bring-Up and Switch-Controlled LED |
| Platform | Blue Pill STM32F103C6T6 (Cortex-M3, 72 MHz) + push-button |
| Tools | STM32CubeMX + STM32CubeIDE, STM32 HAL |

## Circuit Diagram

![Circuit diagram – CAT 8](circuit.png)

*Drawn on [Wokwi](https://wokwi.com). To open and edit it: start a new project on wokwi.com, paste [`wokwi/diagram.json`](wokwi/diagram.json) into its diagram.json tab, and add the `wokwi/*.chip.json` files (if any) with **New file**. The AT89S52, USB-TTL, ULN2003, USBasp and ST-LINK boards are custom chips (pin names added as labels). The ESP-01 part stands in for the Wemos D1 mini and the BMP180 module for the BME280 (same AT firmware / same 4-pin I²C header); the Nucleo-64 graphic is labelled as the F401RE.*

| Pin | CubeMX setting | Connected to |
|---|---|---|
| PC13 | GPIO_Output, push-pull | on-board LED (active-LOW) |
| PA15 | GPIO_Input, pull-up | push-button → GND |
| PA13 / PA14 | SYS: Debug = Serial Wire | ST-LINK (also frees PA15 from JTAG) |
| RCC | HSE crystal → PLL ×9 | 72 MHz |

---

## Task 8A — GPIO Input/Output

**Project folder:** [`CAT08A_GPIO_Input_Output`](CAT08A_GPIO_Input_Output/)

### Working principle
The switch reads 1 when released (pull-up) and 0 when pressed. The loop copies the switch state to the LED: `led_write(button_raw_pressed())`.

### Pseudocode
1. HAL_Init, clock, GPIO.
2. LED OFF.
3. Read PA15; pressed → PC13 = 0, released → PC13 = 1.
4. Repeat.

### Code base
- `button_raw_pressed()` → `HAL_GPIO_ReadPin()`
- `led_write()` → `HAL_GPIO_WritePin()` with active-low levels

### Inference
The LED follows the switch, confirming the input pull-up and active-low output configuration.

---

## Task 8B — Event-Based Control

**Project folder:** [`CAT08B_Event_Toggle_Debounce`](CAT08B_Event_Toggle_Debounce/)

### Working principle
Mechanical contacts bounce for a few ms, which looks like several presses. `debounce_update()` accepts a new level only after the raw input has been unchanged for 25 ms (measured with `HAL_GetTick()`), and returns a press or release edge. The LED toggles only on the press edge.

### Pseudocode
1. Read the raw input and the tick every loop.
2. If the raw level changed, restart the stability timer.
3. If it has been stable for 25 ms and differs from the accepted level, accept it and report the edge.
4. On a press edge toggle PC13 and count it.

### Code base
- `debounce_t` (stable, last_raw, last_change)
- `debounce_update()` → `BTN_PRESS_EDGE` / `BTN_RELEASE_EDGE`
- `HAL_GPIO_TogglePin()`; `toggle_count` for Live Expressions

### Inference
Each press toggles the LED exactly once; bounce is filtered without any blocking delay.

---

## Task 8C — Short Press vs Long Press

**Project folder:** [`CAT08C_Short_vs_Long_Press`](CAT08C_Short_vs_Long_Press/)

### Working principle
Press edge → record start time → keep running → release edge → duration = release − start → classify. < 1 s is short, ≥ 1 s is long.

| Action | Response |
|---|---|
| Short | toggle LED |
| Long | continuous blinking (200 ms) |
| Short while blinking | stop blinking |
| Long again | blink again |

### Pseudocode
1. On a debounced press edge store `press_start_ms`.
2. On release compute the duration.
3. Long → `blinking = 1`.
4. Short → stop blinking if active, otherwise toggle the LED.
5. If blinking and 200 ms passed, toggle and store the time.

### Code base
- `classify_press()`
- `HAL_GetTick()` for both press timing and blink interval — no `HAL_Delay()`

### Inference
Time stamps instead of blocking delays let the program classify presses while the LED keeps blinking.
