# EXPERIMENT DETAILS

**EXP:** CONTINUOUS ASSESSMENT 5

**DATE:** 10/08/2026

**NAME OF EXP:** Hardware Timer and Interrupt-Based Timing for LED Fading, Pulse Generation and Stepper Motor Control

**HARDWARE SETUP / PLATFORM:** AT89S52 development kit (8051 core, 11.0592 MHz crystal) + LED + DSO + ULN2003/28BYJ-48 — Keil µVision 5 (C51), progisp + USBasp ISP programmer, DSO

---

## TASK 1 : Timer 0 Interrupt, 1 s Software Time Base

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

The Timer-0 ISR (vector 1, register bank 1) reloads 0xFC72 (0xFC66 + 12 cycles to make up for the interrupt entry) and counts 1 ms ticks. After 1000 ticks it sets `second_flag`; the main loop toggles the LED when it sees the flag. No delay loop is involved in the timing.

**Pseudocode:**

1. TMOD = Mode 1, load 0xFC72, ET0 = EA = TR0 = 1.
2. ISR: reload, `++ms_ticks`; at 1000 set `second_flag`.
3. Main: if `second_flag`, clear it and toggle P1.1.

**Code Base:**

- Project files: `TASK5A.uvproj` (Keil µVision), `STARTUP.A51`, firmware `Objects/TASK5A.hex`.
- `main.c` — `timer0_isr() interrupt 1 using 1`, `timer0_init_1ms()`

### 3. Inference

The LED changes exactly every second because the hardware timer produces the timing and the software only responds to the event.
