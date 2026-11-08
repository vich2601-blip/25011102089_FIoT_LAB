/*
 * delay.h  -  Software delay library for the AT89S52 (8051)
 * ---------------------------------------------------------------
 * Board crystal : 11.0592 MHz
 * Machine cycle : 12 / 11.0592 MHz = 1.085 us
 *
 * These delays are busy-wait loops, calibrated from the generated
 * assembly (inner loop = DJNZ, 2 machine cycles per pass). Verify with an LED
 * stopwatch test or an oscilloscope and tune DELAY_1MS_INNER if needed.
 */
#ifndef DELAY_H
#define DELAY_H

/* Inner-loop count for ~1 ms. The empty loop compiles to DJNZ (2 cycles):
 * one outer pass = 12 + 4 x 228 = 924 cycles x 1.085 us = 1.003 ms     */
#define DELAY_1MS_INNER   228

void delay_ms(unsigned int ms);     /* ~ms milliseconds                */
void delay_50us(unsigned char n);   /* ~n x 50 microseconds (for LCD)  */

#endif /* DELAY_H */
