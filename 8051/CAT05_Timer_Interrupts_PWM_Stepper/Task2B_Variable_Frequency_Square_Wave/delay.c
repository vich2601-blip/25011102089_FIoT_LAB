/*
 * delay.c  -  Software delay library for the AT89S52 (8051)
 * See delay.h for the timing assumptions.
 */
#include "delay.h"

/* One pass of the outer loop takes about 1 ms:
 * two inner loops of DELAY_1MS_INNER passes, each pass one DJNZ (2 cycles),
 * plus ~12 cycles of loop overhead = 12 + 4 x 228 = 924 cycles = 1.003 ms */
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

/* ~50 us per count: 9 cycles overhead + 18 DJNZ passes x 2 = 45 cycles = 48.8 us */
void delay_50us(unsigned char n)
{
    unsigned char j;

    while (n--)
    {
        for (j = 0; j < 18; j++);
    }
}
