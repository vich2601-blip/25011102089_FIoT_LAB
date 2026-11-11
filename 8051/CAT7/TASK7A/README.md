# EXPERIMENT DETAILS

**EXP:** CONTINUOUS ASSESSMENT 7

**DATE:** 24/08/2026

**NAME OF EXP:** 16×2 LCD Driver Development and Dynamic Display Control

**HARDWARE SETUP / PLATFORM:** AT89S52 development kit (8051 core, 11.0592 MHz crystal) + HD44780 16×2 LCD + 4×4 keypad — Keil µVision 5 (C51), progisp + USBasp ISP programmer, HD44780 datasheet

---

## TASK 7A : LCD Initialisation and Character / String Display

### 1. Circuit Diagram

| LCD | 8051 | Role |
|---|---|---|
| RS | P3.2 | 0 command / 1 data |
| E | P3.3 | latch on falling edge |
| D4–D7 | P3.4–P3.7 | 4-bit bus |
| RW | GND | write only |
| V0 | 10 k pot | contrast |

Keypad: rows P2.7–P2.4, columns P2.3–P2.0.

![Circuit Diagram](circuit.png)

*Drawn on [Wokwi](https://wokwi.com). To open and edit it: start a new project on wokwi.com, paste [`wokwi/diagram.json`](wokwi/diagram.json) into its diagram.json tab, and add the `wokwi/*.chip.json` files (if any) with **New file**. The AT89S52, USB-TTL, ULN2003, USBasp and ST-LINK boards are custom chips (pin names added as labels). The ESP-01 part stands in for the Wemos D1 mini and the BMP180 module for the BME280 (same AT firmware / same 4-pin I²C header); the Nucleo-64 graphic is labelled as the F401RE.*

### 2. Core Logic / Pseudocode / Code Base

**Working Principle:**

`lcd_init()` follows the datasheet 4-bit sequence: wait > 15 ms, nibble 0x3 three times, 0x2, then Function Set 0x28 (4-bit, 2 lines, 5×8), Display Control 0x0C, Entry Mode 0x06, Clear 0x01. Each byte is sent high nibble first; clear/home wait 2 ms, others 50 µs.

**Pseudocode:**

1. `lcd_init()`.
2. `lcd_cmd(0x80)` and `lcd_data('A')`.
3. `lcd_goto(0,2)`, `lcd_puts("CAT7 LCD TEST")`.
4. `lcd_goto(1,0)`, `lcd_puts("8051 4-BIT MODE")`.

**Code Base:**

- Project files: `TASK7A.uvproj` (Keil µVision), `STARTUP.A51`, firmware `Objects/TASK7A.hex`.
- `lcd.h` — command constants
- `lcd.c` — `lcd_init()`, `lcd_cmd()`, `lcd_data()`
- `main.c`

**Expected Output:**

```
A CAT7 LCD TEST
8051 4-BIT MODE
```

### 3. Inference

The command values were taken straight from the HD44780 instruction table, and text appears on both rows.
