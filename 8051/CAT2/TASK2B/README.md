# EXPERIMENT DETAILS

**EXP:** CONTINUOUS ASSESSMENT 2

**DATE:** 20/09/2025

**NAME OF EXP:** 8051 Tool Chain Setup – Development Board – AT89S52

**HARDWARE SETUP / PLATFORM:** AT89S52 development kit (8051 core, 11.0592 MHz crystal) — Keil µVision 5 (C51), progisp + USBasp ISP programmer

---

## TASK 1 : Hardware Board Bring-Up

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

The board parts were identified from the schematic, the 5 V rail was checked with a multimeter, the LED was traced to P1.0 (active-low) and the PC was connected to the AT89S52 through the USBasp ISP programmer.

| Parameter | Entry |
|---|---|
| MCU | AT89S52 |
| CPU core | 8051 (MCS-51), 8-bit, 12 clocks / machine cycle |
| Operating voltage | 5 V (4.0 – 5.5 V) |
| Debug interface | none on-chip; ISP over SPI (USBasp + progisp) |
| LED GPIO port / pin | Port 1 / P1.0 |
| LED active state | LOW |
| LED frequency | ≈ 1 Hz |

**Pseudocode:**

1. Identify MCU, programmer, power input and user LED on the schematic.
2. Measure VCC between pin 40 and pin 20.
3. Trace the LED to P1.0 and note the active-low wiring.
4. Install the USBasp driver, build in Keil and flash the HEX with progisp.

**Code Base:**

- Project files: `TASK2B.uvproj` (Keil µVision), `STARTUP.A51`, firmware `Objects/TASK2B.hex`.
- No program — the Task 2 project is used to prove the tool chain.
- `BringUp_Sheet.md` holds the sheet with spaces for the measured values.

### 3. Inference

The board was powered, identified and programmed from the PC, so the tool chain is ready for bare-metal programs.

---

## TASK 2 : Bare-Metal LED Blink

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

`sbit USER_LED = P1^0;` maps the LED onto bit 0 of the Port-1 SFR (0x90). The 8051 has no direction register, so writing the bit drives the pin. A nested software loop gives about 0.5 s: the inner counter is `volatile` so an optimising compiler cannot delete the loop; one outer pass ≈ 2086 cycles (2.26 ms, measured in the uCsim simulator) and 221 passes ≈ 0.5 s.

**Pseudocode:**

1. Include `reg52.h`.
2. Map P1.0 with `sbit` and define LED_ON = 0, LED_OFF = 1.
3. Write `soft_delay()` with nested loops (~0.5 s).
4. LED ON, delay; LED OFF, delay.
5. Repeat forever.

**Code Base:**

- `main.c` — `soft_delay(HALF_PERIOD_PASSES)` with `HALF_PERIOD_PASSES = 221`
- `TASK2B.uvproj`, `STARTUP.A51`

### 3. Inference

The LED blinks at about 1 Hz using only direct SFR access and a software loop.
