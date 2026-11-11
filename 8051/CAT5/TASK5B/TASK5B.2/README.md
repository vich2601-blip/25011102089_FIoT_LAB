# EXPERIMENT DETAILS

**EXP:** CONTINUOUS ASSESSMENT 5

**DATE:** 10/08/2026

**NAME OF EXP:** Hardware Timer and Interrupt-Based Timing for LED Fading, Pulse Generation and Stepper Motor Control

**HARDWARE SETUP / PLATFORM:** AT89S52 development kit (8051 core, 11.0592 MHz crystal) + LED + DSO + ULN2003/28BYJ-48 — Keil µVision 5 (C51), progisp + USBasp ISP programmer, DSO

---

## TASK 2B : Variable-Frequency Square Wave

### 1. Circuit Diagram

> **Note:** this diagram shows all CAT 5 tasks together. P1.1 drives the LED in Tasks 1 and 2 (LED, PWM, square waves) and is ULN2003 IN2 in Task 3 (stepper), so connect only one of them at a time.

Crystal 11.0592 MHz → machine cycle 1.085 µs → Timer 0 → overflow → **interrupt** → software time base → action

| Time base | Counts | Setting |
|---|---|---|
| 1 ms | 922 | Mode 1, TH0:TL0 = 0xFC66 (+12 for ISR entry → 0xFC72), reloaded in the ISR |
| 100 µs | 92 | Mode 2 auto-reload, TH0 = 0xA4 |

![Circuit Diagram](circuit.png)

*Drawn on [Wokwi](https://wokwi.com). To open and edit it: start a new project on wokwi.com, paste [`wokwi/diagram.json`](wokwi/diagram.json) into its diagram.json tab, and add the `wokwi/*.chip.json` files (if any) with **New file**. The AT89S52, USB-TTL, ULN2003, USBasp and ST-LINK boards are custom chips (pin names added as labels). The ESP-01 part stands in for the Wemos D1 mini and the BMP180 module for the BME280 (same AT firmware / same 4-pin I²C header); the Nucleo-64 graphic is labelled as the F401RE.*

### 2. Core Logic / Pseudocode / Code Base

**Working Principle:**

The ISR toggles P1.1, so the frequency depends only on the reload value: half-period counts N = 460800 / f.

| Mode | f | N | Reload |
|---|---|---|---|
| 1 | 100 Hz | 4608 | 0xEE00 |
| 2 | 500 Hz | 922 | 0xFC66 |
| 3 | 1 kHz | 461 | 0xFE33 |
| 4 | 2 kHz | 230 | 0xFF1A |
| 5 | 5 kHz | 92 | 0xFFA4 |

The table shows the ideal reload. The code adds `ISR_LATENCY` (12 cycles) to it, because the timer only restarts once the ISR has reloaded it: reload = 65536 − N + 12. When the mode changes, Timer 0 is stopped, reloaded and restarted so the new frequency starts immediately. Measured in uCsim: mode 1 = 100.2 Hz.

**Pseudocode:**

1. Compute reload = 65536 − N + ISR_LATENCY for the mode.
2. ISR: reload, toggle P1.1.
3. Key press → next mode, show 1–5 on the 7-segment.

**Code Base:**

- Project files: `TASK5B_2.uvproj` (Keil µVision), `STARTUP.A51`, firmware `Objects/TASK5B_2.hex`.
- `main.c` — `HALF_PERIOD[]` table, `set_mode()`

### 3. Inference

All five frequencies come from the same interrupt routine; only the timer reload changes.
