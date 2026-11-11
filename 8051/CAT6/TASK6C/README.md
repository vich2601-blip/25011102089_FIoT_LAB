# EXPERIMENT DETAILS

**EXP:** CONTINUOUS ASSESSMENT 6

**DATE:** 17/08/2026

**NAME OF EXP:** UART Driver and Serial Debugging

**HARDWARE SETUP / PLATFORM:** AT89S52 development kit (8051 core, 11.0592 MHz crystal) + USB-to-TTL converter + PuTTY — Keil µVision 5 (C51), progisp + USBasp ISP programmer, PuTTY

---

## TASK C : Interrupt-Driven RX Ring Buffer

### 1. Circuit Diagram

| 8051 | USB-to-TTL |
|---|---|
| P3.1 TXD | RX |
| P3.0 RXD | TX |
| GND | GND |

PuTTY: 9600 baud, 8 data, no parity, 1 stop.

![Circuit Diagram](circuit.png)

*Drawn on [Wokwi](https://wokwi.com). To open and edit it: start a new project on wokwi.com, paste [`wokwi/diagram.json`](wokwi/diagram.json) into its diagram.json tab, and add the `wokwi/*.chip.json` files (if any) with **New file**. The AT89S52, USB-TTL, ULN2003, USBasp and ST-LINK boards are custom chips (pin names added as labels). The ESP-01 part stands in for the Wemos D1 mini and the BMP180 module for the BME280 (same AT firmware / same 4-pin I²C header); the Nucleo-64 graphic is labelled as the F401RE.*

### 2. Core Logic / Pseudocode / Code Base

**Working Principle:**

The serial interrupt (vector 4) stores each received byte in a 16-slot ring buffer (one slot is kept free to tell "full" from "empty", so it holds 15 bytes) (`head` written by the ISR, `tail` read by main) and clears TI for the transmitter. The main loop is free to do other work (an LED heartbeat) and collects a line until Enter.

**Pseudocode:**

1. `uart_init()` with ES = EA = 1.
2. ISR: on RI push SBUF into the buffer; on TI clear `tx_busy`.
3. Main: while data is available, echo it and store it in `line`.
4. On Enter print `Got N chars: <line>`.
5. Toggle the heartbeat LED in the background.

**Code Base:**

- Project files: `TASK6C.uvproj` (Keil µVision), `STARTUP.A51`, firmware `Objects/TASK6C.hex`.
- `uart.c` / `uart.h` — ISR, ring buffer, `uart_available()`, `uart_getc()`
- `main.c` — line handling and heartbeat

**Expected Output:**

```
> hello
Got 5 chars: hello
```

**Supporting notes:**

| Stage | Finding |
|---|---|
| Serial pins | TXD P3.1, RXD P3.0 (alternate function of Port 3) |
| Mode register | SCON; Mode 1 = 8-bit UART, variable baud; REN = 1 → `SCON = 0x50` |
| TX register | SBUF (separate TX and RX buffers at one address) |
| TX complete | TI set by hardware, cleared by software |
| Baud generator | Timer 1, Mode 2 auto-reload |
| 9600 baud | 11059200 / (384 × (256 − TH1)) → TH1 = 0xFD |
| RX complete | RI set, byte in SBUF |

### 3. Inference

With interrupt-driven reception the main program can be busy for up to 15 character times (≈ 15 ms at 9600 baud) without losing data; beyond that the 16-byte buffer is full and further bytes are dropped and counted (`uart_overflows()`, printed after each line).
