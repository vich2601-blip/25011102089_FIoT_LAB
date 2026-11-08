# CAT 6 — UART Driver and Serial Debugging

| Field | Details |
|---|---|
| Experiment | Continuous Lab Evaluation 6 |
| Date | 17/08 (Week 5) |
| Experiment name | UART Driver and Serial Debugging |
| Platform | AT89S52 development kit (8051 core, 11.0592 MHz crystal) + USB-to-TTL converter + PuTTY |
| Tools | Keil µVision 5 (C51), progisp + USBasp ISP programmer, PuTTY |

## Circuit Diagram

![Circuit diagram – CAT 6](circuit.png)

*Drawn on [Wokwi](https://wokwi.com). To open and edit it: start a new project on wokwi.com, paste [`wokwi/diagram.json`](wokwi/diagram.json) into its diagram.json tab, and add the `wokwi/*.chip.json` files (if any) with **New file**. The AT89S52, USB-TTL, ULN2003, USBasp and ST-LINK boards are custom chips (pin names added as labels). The ESP-01 part stands in for the Wemos D1 mini and the BMP180 module for the BME280 (same AT firmware / same 4-pin I²C header); the Nucleo-64 graphic is labelled as the F401RE.*

| 8051 | USB-to-TTL |
|---|---|
| P3.1 TXD | RX |
| P3.0 RXD | TX |
| GND | GND |

PuTTY: 9600 baud, 8 data, no parity, 1 stop.

| Stage | Finding |
|---|---|
| Serial pins | TXD P3.1, RXD P3.0 (alternate function of Port 3) |
| Mode register | SCON; Mode 1 = 8-bit UART, variable baud; REN = 1 → `SCON = 0x50` |
| TX register | SBUF (separate TX and RX buffers at one address) |
| TX complete | TI set by hardware, cleared by software |
| Baud generator | Timer 1, Mode 2 auto-reload |
| 9600 baud | 11059200 / (384 × (256 − TH1)) → TH1 = 0xFD |
| RX complete | RI set, byte in SBUF |

---

## Task A — UART Initialisation and Character Transmission

**Project folder:** [`TaskA_UART_Init_and_Transmit`](TaskA_UART_Init_and_Transmit/)

### Working principle
`uart_init()` sets SCON = 0x50, Timer 1 Mode 2 with TH1 = 0xFD and starts it. `uart_putc()` writes SBUF and waits for TI. A single `U` (0x55) is sent first as a known test character, then the string repeats.

### Pseudocode
1. `uart_init()`.
2. Send `U` and CR LF.
3. Loop: send `Hello from 8051\r\n`, wait 0.5 s.

### Code base
- `main.c` — `uart_init()`, `uart_putc()`, `uart_puts()`

### Expected output
```
U
Hello from 8051
Hello from 8051
...
```

### Inference
The UART configuration and baud rate match PuTTY, confirming the transmit path.

---

## Task B — UART Echo

**Project folder:** [`TaskB_UART_Echo`](TaskB_UART_Echo/)

### Working principle
`uart_getc()` waits for RI, reads SBUF and clears RI; the character is sent straight back (CR is followed by LF).

### Pseudocode
1. `uart_init()` and a ready message.
2. Wait for a character.
3. Send it back; add LF after CR.
4. Repeat.

### Code base
- `main.c` — `uart_getc()` + `uart_putc()`

### Expected output
```
typed: ABC123      → shown: ABC123
typed: Hello 8051  → shown: Hello 8051
```

### Inference
PC → 8051 → PC works, so both receive and transmit paths are correct.

---

## Task C — Interrupt-Driven RX Ring Buffer

**Project folder:** [`TaskC_Interrupt_Driven_RX_Ring_Buffer`](TaskC_Interrupt_Driven_RX_Ring_Buffer/)

### Working principle
The serial interrupt (vector 4) stores each received byte in a 16-byte ring buffer (`head` written by the ISR, `tail` read by main) and clears TI for the transmitter. The main loop is free to do other work (an LED heartbeat) and collects a line until Enter.

### Pseudocode
1. `uart_init()` with ES = EA = 1.
2. ISR: on RI push SBUF into the buffer; on TI clear `tx_busy`.
3. Main: while data is available, echo it and store it in `line`.
4. On Enter print `Got N chars: <line>`.
5. Toggle the heartbeat LED in the background.

### Code base
- `uart.c` / `uart.h` — ISR, ring buffer, `uart_available()`, `uart_getc()`
- `main.c` — line handling and heartbeat

### Expected output
```
> hello
Got 5 chars: hello
```

### Inference
With interrupt-driven reception the main program can be busy for up to 15 character times (≈ 15 ms at 9600 baud) without losing data; beyond that the 16-byte buffer is full and further bytes are dropped and counted (`uart_overflows()`, printed after each line).
