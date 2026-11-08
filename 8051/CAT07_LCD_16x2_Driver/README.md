# CAT 7 — 16×2 LCD Driver Development and Dynamic Display Control

| Field | Details |
|---|---|
| Experiment | Continuous Lab Evaluation 7 |
| Date | 24/08 (Week 6) |
| Experiment name | 16×2 LCD Driver Development and Dynamic Display Control |
| Platform | AT89S52 development kit (8051 core, 11.0592 MHz crystal) + HD44780 16×2 LCD + 4×4 keypad |
| Tools | Keil µVision 5 (C51), progisp + USBasp ISP programmer, HD44780 datasheet |

## Circuit Diagram

![Circuit diagram – CAT 7](circuit.png)

*Drawn on [Wokwi](https://wokwi.com). To open and edit it: start a new project on wokwi.com, paste [`wokwi/diagram.json`](wokwi/diagram.json) into its diagram.json tab, and add the `wokwi/*.chip.json` files (if any) with **New file**. The AT89S52, USB-TTL, ULN2003, USBasp and ST-LINK boards are custom chips (pin names added as labels). The ESP-01 part stands in for the Wemos D1 mini and the BMP180 module for the BME280 (same AT firmware / same 4-pin I²C header); the Nucleo-64 graphic is labelled as the F401RE.*

| LCD | 8051 | Role |
|---|---|---|
| RS | P3.2 | 0 command / 1 data |
| E | P3.3 | latch on falling edge |
| D4–D7 | P3.4–P3.7 | 4-bit bus |
| RW | GND | write only |
| V0 | 10 k pot | contrast |

Keypad: rows P2.7–P2.4, columns P2.3–P2.0.

---

## Task 7A — LCD Initialisation and Character / String Display

**Project folder:** [`Task7A_LCD_Init_Char_String`](Task7A_LCD_Init_Char_String/)

### Working principle
`lcd_init()` follows the datasheet 4-bit sequence: wait > 15 ms, nibble 0x3 three times, 0x2, then Function Set 0x28 (4-bit, 2 lines, 5×8), Display Control 0x0C, Entry Mode 0x06, Clear 0x01. Each byte is sent high nibble first; clear/home wait 2 ms, others 50 µs.

### Pseudocode
1. `lcd_init()`.
2. `lcd_cmd(0x80)` and `lcd_data('A')`.
3. `lcd_goto(0,2)`, `lcd_puts("CAT7 LCD TEST")`.
4. `lcd_goto(1,0)`, `lcd_puts("8051 4-BIT MODE")`.

### Code base
- `lcd.h` — command constants
- `lcd.c` — `lcd_init()`, `lcd_cmd()`, `lcd_data()`
- `main.c`

### Expected output
```
A CAT7 LCD TEST
8051 4-BIT MODE
```

### Inference
The command values were taken straight from the HD44780 instruction table, and text appears on both rows.

---

## Task 7B — Cursor Positioning and Dynamic Value Update

**Project folder:** [`Task7B_Cursor_Position_Dynamic_Value`](Task7B_Cursor_Position_Dynamic_Value/)

### Working principle
`lcd_goto(row, col)` sends 0x80 + (row ? 0x40 : 0) + col. `lcd_print_number()` stores `value % 10 + '0'` digits and prints them most significant first. Labels are written once; only the number fields are blanked and rewritten every 500 ms.

### Pseudocode
1. Write `Counter:` and `Up time:` once.
2. Every 500 ms blank and rewrite the counter field.
3. Every second blank and rewrite the up-time field.

### Code base
- `lcd_goto()`, `lcd_puts()`, `lcd_print_number()`, `lcd_clear_field()`

### Expected output
```
Counter:  137
Up time: 68   s
```

### Inference
Any position can be addressed, so values update without redrawing the whole display.

---

## Task 7C — 4×4 Keypad Character Movement

**Project folder:** [`Task7C_Keypad_Character_Movement`](Task7C_Keypad_Character_Movement/)

### Working principle
`*` starts at (0,0). Keys 2/8/4/6 move it up/down/left/right inside rows 0–1, columns 0–15; key 5 returns home. The old cell is overwritten with a space, the new cell gets `*`, and the program waits for release so one press = one step.

### Pseudocode
1. Draw `*` at (0,0).
2. Scan the keypad; skip if none.
3. Erase the old position.
4. Update row/col within limits.
5. Draw at the new position; wait for release.

### Code base
- `keypad_scan()` — 4×4 scan with `KEYMAP[4][4]`
- LCD driver from 7A

### Inference
Combining keypad scanning with cursor addressing gives a simple interactive display.
