# CAT 2 - Task 1 : Hardware Board Bring-Up Sheet

No program is needed for this task. Fill in the measured values in the lab.
The Task 2 program is then used to check the toolchain and the board.

| Parameter            | Student Entry                                                  |
|----------------------|----------------------------------------------------------------|
| MCU                  | AT89S52 (Microchip/Atmel)                                      |
| CPU Core             | 8051 (MCS-51), 8-bit, 12 clocks per machine cycle               |
| Operating Voltage    | 5 V (datasheet range 4.0 V - 5.5 V)                            |
| Measured VCC (pin 40 to pin 20) | ______ V  (measure with the multimeter)            |
| Crystal              | 11.0592 MHz -> machine cycle = 1.085 us                        |
| Debug Interface      | No on-chip debugger. Programmed by ISP (SPI): P1.5 MOSI, P1.6 MISO, P1.7 SCK, RST, through the USBasp programmer and progisp |
| LED GPIO Port        | Port 1                                                         |
| LED GPIO Pin         | P1.0 (on-board user LED)                                       |
| LED Active State     | Active-LOW (LED is ON when P1.0 = 0)                           |
| LED Frequency (Hz)   | ~1 Hz (0.5 s ON, 0.5 s OFF) - measured: ______ Hz              |

## Steps
1. Identify the MCU, the ISP programmer (USBasp), the power input and the user LED from the board schematic
   (`04_Tools_and_Drivers/8051_Dev_Board_Schematic_Codes_Programmer_Drivers/Amotech Labs 8051 Board Schematics`).
2. Power the board and measure VCC between pin 40 and pin 20 with the digital multimeter.
3. Trace the LED to P1.0 on the schematic and note that it is wired active-low.
4. Install the USBasp driver (Zadig / `USBasp-Drivers_Windows_11`), build the Task 2 project in Keil
   (F7) and flash `Objects/*.hex` with progisp.
