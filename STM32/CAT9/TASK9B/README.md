# EXPERIMENT DETAILS

**EXP:** CONTINUOUS ASSESSMENT 9

**DATE:** 28/09/2026

**NAME OF EXP:** STM32 UART Command Controlled LED

**HARDWARE SETUP / PLATFORM:** Blue Pill STM32F103C6T6 (Cortex-M3, 72 MHz) + USB-to-TTL + PuTTY — STM32CubeMX + Keil MDK-ARM (STM32CubeIDE project also included), STM32 HAL, PuTTY

---

## TASK 9B : String-Based Application Logic

### 1. Circuit Diagram

| STM32 | USB-to-TTL |
|---|---|
| PA2 USART2_TX | RX |
| PA3 USART2_RX | TX |
| GND | GND |

USART2 asynchronous, **115200-8-N-1**. LED on PC13.

![Circuit Diagram](circuit.png)

*Drawn on [Wokwi](https://wokwi.com). To open and edit it: start a new project on wokwi.com, paste [`wokwi/diagram.json`](wokwi/diagram.json) into its diagram.json tab, and add the `wokwi/*.chip.json` files (if any) with **New file**. The AT89S52, USB-TTL, ULN2003, USBasp and ST-LINK boards are custom chips (pin names added as labels). The ESP-01 part stands in for the Wemos D1 mini and the BMP180 module for the BME280 (same AT firmware / same 4-pin I²C header); the Nucleo-64 graphic is labelled as the F401RE.*

### 2. Core Logic / Pseudocode / Code Base

**Working Principle:**

Characters are echoed and collected until CR/LF. The command is trimmed and upper-cased, then compared with `strcmp()`.

| Command | Action |
|---|---|
| `LED ON` / `LED OFF` / `LED TOGGLE` | drive PC13 |
| `STATUS` | report state |
| `CRLF TEST` | send the CR/LF test strings |
| `HELP` | list commands |

| String | PuTTY result |
|---|---|
| `"HELLO\r\n"` | new line |
| `"HELLO\r"` | back to start, next text overwrites |
| `"HELLO\n"` | down one line, same column |
| `"HELLO\r\nWORLD\r\n"` | two lines |

**Pseudocode:**

1. LED OFF, print prompt.
2. Receive a byte.
3. CR/LF and buffer not empty → terminate, `handle_command()`, clear buffer.
4. Backspace → remove last char.
5. Otherwise store and echo.

**Code Base:**

- Project files: `CAT09B_String_Command_LED.ioc` (CubeMX configuration), `MDK-ARM/CAT09B_String_Command_LED.uvprojx` (Keil MDK), `.project` / `.cproject` (STM32CubeIDE); application code in `Core/Src/main.c`.
- `normalise()` — trim + upper-case
- `handle_command()`, `crlf_demo()`

**Expected Output:**

```
> led on
OK: LED is ON
> blink
ERR: unknown command 'BLINK' (type HELP)
```

### 3. Inference

Line-based parsing turns the UART into a text command interface on top of HAL.
