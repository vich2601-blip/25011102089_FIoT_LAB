/*
 * delay.c  -  Software delay library for the AT89S52 (8051)
 * See delay.h for the timing assumptions.
 */
#include "delay.h"

/* One pass of the outer loop takes about 1 ms:
 * two inner loops x 153 passes x ~3 machine cycles = ~920 cycles
 * 920 x 1.085 us = ~1.0 ms                                       */
void delay_ms(unsigned int ms)
{
    unsigned int  i;
    unsigned char j;

    for (i = 0; i < ms; i++)
    {
        for (j = 0; j < DELAY_1MS_INNER; j++);
        for (j = 0; j < DELAY_1MS_INNER; j++);
    }
}

/* ~50 us per count: 15 passes x 3 cycles = 45 cycles + call overhead */
void delay_50us(unsigned char n)
{
    unsigned char j;

    while (n--)
    {
        for (j = 0; j < 15; j++);
    }
}
