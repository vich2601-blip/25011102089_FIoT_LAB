/*
 * delay.h  -  Software delay library for the AT89S52 (8051)
 * ---------------------------------------------------------------
 * Board crystal : 11.0592 MHz
 * Machine cycle : 12 / 11.0592 MHz = 1.085 us
 *
 * These delays are busy-wait loops. They are only "approximately"
 * calibrated (Keil C51, optimisation level 8). Verify with an LED
 * stopwatch test or an oscilloscope and tune DELAY_1MS_INNER if needed.
 */
#ifndef DELAY_H
#define DELAY_H

/* Inner-loop count that gives ~1 ms (2 x 153 x 3 cycles ~ 920 cycles) */
#define DELAY_1MS_INNER   153

void delay_ms(unsigned int ms);     /* ~ms milliseconds                */
void delay_50us(unsigned char n);   /* ~n x 50 microseconds (for LCD)  */

#endif /* DELAY_H */
