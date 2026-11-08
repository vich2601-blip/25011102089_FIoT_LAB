# 8051 Experiments (CAT 2 – CAT 7)

Bare-metal Embedded C on the AT89S52 (11.0592 MHz) with Keil µVision. Every task folder is a complete Keil project (`*.uvproj`, `STARTUP.A51`, sources) with a ready-to-flash HEX in `Objects/`.

**About the HEX files:** they were built with SDCC 4.6 (open-source 8051 compiler) from the same sources, keywords mapped automatically (`sbit` → `P1_0`, `interrupt n using m` → `__interrupt(n) __using(m)`, `code` → `__code`). Timing was checked in the uCsim 8051 simulator at 11.0592 MHz: CAT2 blink = 1.000 s period, `delay_ms()` = 1.003 ms, CAT5 timer tick → 1.000 s toggle, 100 Hz mode = 100.2 Hz. Building in Keil (F7) overwrites them with Keil-compiled HEX files.

| CAT | Folder |
|---|---|
| 2 | [CAT02_LED_Blink_and_Delay_Library](CAT02_LED_Blink_and_Delay_Library/) |
| 3 | [CAT03_Button_LED_Pattern_Generator](CAT03_Button_LED_Pattern_Generator/) |
| 4 | [CAT04_Keypad_7Segment_HMI](CAT04_Keypad_7Segment_HMI/) |
| 5 | [CAT05_Timer_Interrupts_PWM_Stepper](CAT05_Timer_Interrupts_PWM_Stepper/) |
| 6 | [CAT06_UART_Driver_Serial_Debugging](CAT06_UART_Driver_Serial_Debugging/) |
| 7 | [CAT07_LCD_16x2_Driver](CAT07_LCD_16x2_Driver/) |
