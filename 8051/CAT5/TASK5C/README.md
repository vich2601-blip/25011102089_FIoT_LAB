# EXPERIMENT DETAILS

**EXP:** CONTINUOUS ASSESSMENT 5

**DATE:** 10/08/2026

**NAME OF EXP:** Hardware Timer and Interrupt-Based Timing for LED Fading, Pulse Generation and Stepper Motor Control

**HARDWARE SETUP / PLATFORM:** AT89S52 development kit (8051 core, 11.0592 MHz crystal) + LED + DSO + ULN2003/28BYJ-48 — Keil µVision 5 (C51), progisp + USBasp ISP programmer, DSO

---

## TASK 3 : Stepper Motor Speed and Direction (optional)

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

The 1 ms ISR counts to `step_ms` and then outputs the next wave-drive step (1000 → 0100 → 0010 → 0001) on P1.0–P1.3, so the timer interval sets the speed. Keys: 1 slow (100 ms), 2 medium (50 ms), 3 fast (10 ms), 4 reverse, 5 stop/start.

**Pseudocode:**

1. Timer 0, 1 ms interrupt.
2. ISR: `++ms_count`; at `step_ms` advance `phase` ±1 and write `STEP_SEQ[phase]`.
3. Main: read the keypad and change `step_ms`, `clockwise` or `running`.

**Code Base:**

- Project files: `TASK5C.uvproj` (Keil µVision), `STARTUP.A51`, firmware `Objects/TASK5C.hex`.
- `main.c` — `STEP_SEQ[]`, `keypad_get()`

### 3. Inference

Motor speed and direction are set only by the interrupt interval and step order, independent of the key-handling code.
