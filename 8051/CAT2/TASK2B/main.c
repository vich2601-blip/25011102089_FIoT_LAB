/*
 * CAT 2 - Task 2 : Bare-Metal LED Blink (AT89S52)
 * ---------------------------------------------------------------
 * Blinks the on-board user LED at ~1 Hz using ONLY direct SFR access
 * (no libraries). P1.0 is mapped onto bit 0 of the Port 1 SFR (0x90)
 * with 'sbit', so writing LED = 0/1 changes the pin directly.
 *
 * Board  : AT89S52 development kit, 11.0592 MHz crystal
 * LED    : P1.0, ACTIVE-LOW (LED glows when the pin is 0)
 * Timing : 0.5 s ON + 0.5 s OFF  ->  1 Hz
 */
#include <reg52.h>              /* SFR definitions for the 8052 family */

sbit USER_LED = P1^0;           /* on-board LED                         */

#define LED_ON    0             /* active-low                           */
#define LED_OFF   1

/* Number of outer-loop passes for ~0.5 s.
 * The inner counter is volatile, so every pass really executes:
 * one outer pass = ~2086 machine cycles (230 inner passes x ~9 cycles)
 *                = 2.26 ms at 1.085 us per cycle
 * 0.5 s / 2.26 ms = 221 passes  (measured in the uCsim 8051 simulator)   */
#define HALF_PERIOD_PASSES   221

/* Simple software-loop delay (blocking).
 * Not "static": a static function with no side effects may be removed by an
 * optimising compiler, which would make the LED toggle too fast to see.   */
void soft_delay(unsigned int passes)
{
    unsigned int  i;
    volatile unsigned char j;   /* volatile: the compiler must really run the loop */

    for (i = 0; i < passes; i++)
    {
        for (j = 0; j < 230; j++)
        {
            /* do nothing - just burn machine cycles */
        }
    }
}

void main(void)
{
    /* The 8051 has no direction register: a port pin becomes an output
       as soon as we write to it. Start with the LED OFF.               */
    USER_LED = LED_OFF;

    while (1)
    {
        USER_LED = LED_ON;
        soft_delay(HALF_PERIOD_PASSES);

        USER_LED = LED_OFF;
        soft_delay(HALF_PERIOD_PASSES);
    }
}
