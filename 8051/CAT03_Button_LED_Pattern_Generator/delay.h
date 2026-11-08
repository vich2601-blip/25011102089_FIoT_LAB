/*
 * delay.h  -  Software delay library for the AT89S52 (8051)
 * ---------------------------------------------------------------
 * Board crystal : 11.0592 MHz
 * Machine cycle : 12 / 11.0592 MHz = 1.085 us
 *
 * These delays are busy-wait loops with a volatile counter, so no optimiser
 * (Keil level 8 included) can remove them. Calibrated from the generated
 * assembly and measured in the uCsim simulator (SDCC). Keil code may differ
 * by a few percent: check once with a stopwatch or DSO. Verify with an LED
 * stopwatch test or an oscilloscope and tune DELAY_1MS_INNER if needed.
 */
#ifndef DELAY_H
#define DELAY_H

/* Inner-loop count for ~1 ms. The loop counter is volatile, so each pass is
 * 9 machine cycles (load, compare, increment, store, jump):
 * one outer pass = 22 + 18 x 50 = 922 cycles x 1.085 us = 1.000 ms    */
#define DELAY_1MS_INNER   50

void delay_ms(unsigned int ms);     /* ~ms milliseconds                */
void delay_50us(unsigned char n);   /* ~n x 50 microseconds (for LCD)  */

#endif /* DELAY_H */
