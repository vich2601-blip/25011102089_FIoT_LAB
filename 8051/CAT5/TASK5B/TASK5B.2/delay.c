/*
 * delay.c  -  Software delay library for the AT89S52 (8051)
 * See delay.h for the timing assumptions.
 */
#include "delay.h"

/* One pass of the outer loop takes about 1 ms:
 * two inner loops of DELAY_1MS_INNER passes x 9 cycles + ~22 cycles overhead
 * = 22 + 18 x 50 = 922 cycles = 1.000 ms at 11.0592 MHz */
void delay_ms(unsigned int ms)
{
    unsigned int  i;
    volatile unsigned char j;   /* volatile: no optimiser may remove the loop */

    for (i = 0; i < ms; i++)
    {
        for (j = 0; j < DELAY_1MS_INNER; j++);
        for (j = 0; j < DELAY_1MS_INNER; j++);
    }
}

/* ~52 us per count: 12 cycles overhead + 4 passes x 9 = 48 cycles (HD44780 needs >= 37 us) */
void delay_50us(unsigned char n)
{
    volatile unsigned char j;

    while (n--)
    {
        for (j = 0; j < 4; j++);
    }
}
