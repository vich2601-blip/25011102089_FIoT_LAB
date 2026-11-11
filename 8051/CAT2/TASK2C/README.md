# EXPERIMENT DETAILS

**EXP:** CONTINUOUS ASSESSMENT 2

**DATE:** 20/09/2025

**NAME OF EXP:** 8051 Tool Chain Setup – Development Board – AT89S52

**HARDWARE SETUP / PLATFORM:** AT89S52 development kit (8051 core, 11.0592 MHz crystal) — Keil µVision 5 (C51), progisp + USBasp ISP programmer

---

## TASK 3 : Custom Library Function for Delay

### 1. Circuit Diagram

| Signal | Pin | Notes |
|---|---|---|
| User LED | P1.0 | on-board, **active-LOW** (0 = ON) |
| ISP | P1.5 MOSI, P1.6 MISO, P1.7 SCK, RST | USBasp 10-pin header |
| Supply | VCC pin 40, GND pin 20 | 5 V |

![Circuit Diagram](circuit.png)

*Drawn on [Wokwi](https://wokwi.com). To open and edit it: start a new project on wokwi.com, paste [`wokwi/diagram.json`](wokwi/diagram.json) into its diagram.json tab, and add the `wokwi/*.chip.json` files (if any) with **New file**. The AT89S52, USB-TTL, ULN2003, USBasp and ST-LINK boards are custom chips (pin names added as labels). The ESP-01 part stands in for the Wemos D1 mini and the BMP180 module for the BME280 (same AT firmware / same 4-pin I²C header); the Nucleo-64 graphic is labelled as the F401RE.*

### 2. Core Logic / Pseudocode / Code Base

**Working Principle:**

The delay is moved into `delay.h` / `delay.c`. `delay_ms(n)` repeats a ~1 ms block: two loops of 50 passes with a `volatile` counter (9 cycles per pass, so no optimiser can remove them) + overhead = 22 + 18 × 50 = 922 cycles ≈ 1.000 ms (measured in the uCsim simulator; check once under Keil, whose code may differ by a few percent). `delay_50us()` is added for short device timings (used later by the LCD driver).

**Pseudocode:**

1. Declare `delay_ms()` and `delay_50us()` in `delay.h`.
2. Implement the calibrated loops in `delay.c`.
3. In `main.c` include `delay.h`.
4. LED ON, `delay_ms(500)`; LED OFF, `delay_ms(500)`.
5. Repeat.

**Code Base:**

- Project files: `TASK2C.uvproj` (Keil µVision), `STARTUP.A51`, firmware `Objects/TASK2C.hex`.
- `delay.h` — interface and timing notes
- `delay.c` — calibrated loops
- `main.c` — 1 Hz blink with `delay_ms(BLINK_HALF_MS)`

### 3. Inference

With the delay in its own library the application states the time it wants in milliseconds, and the same library is reused in CAT 3 – 7.
