# EXPERIMENT DETAILS

**EXP:** CONTINUOUS ASSESSMENT 5

**DATE:** 10/08/2026

**NAME OF EXP:** Hardware Timer and Interrupt-Based Timing for LED Fading, Pulse Generation and Stepper Motor Control

**HARDWARE SETUP / PLATFORM:** AT89S52 development kit (8051 core, 11.0592 MHz crystal) + LED + DSO + ULN2003/28BYJ-48 — Keil µVision 5 (C51), progisp + USBasp ISP programmer, DSO

---

## TASK 2A : Software PWM LED Fading

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

A 100 µs interrupt drives a 10-slot PWM (1 ms period → 1 kHz, no flicker). `duty` (0–10) sets how many slots the LED is on. Every 100 ms the duty steps by 10 %: 0 → 100 → 0 % in 2 s.

**Pseudocode:**

1. Timer 0 Mode 2, TH0 = TL0 = 0xA4.
2. ISR: advance `pwm_slot` 0–9, LED = (`pwm_slot < duty`); count 1000 ticks → `step_flag`.
3. Main: on `step_flag` add `direction` to `duty`, reverse at 0 and 10.

**Code Base:**

- Project files: `TASK5B_1.uvproj` (Keil µVision), `STARTUP.A51`, firmware `Objects/TASK5B_1.hex`.
- `main.c` — PWM and fade time base in one short ISR

### 3. Inference

The LED brightens and dims smoothly; the brightness follows the duty cycle while the 1 kHz PWM frequency stays constant.
