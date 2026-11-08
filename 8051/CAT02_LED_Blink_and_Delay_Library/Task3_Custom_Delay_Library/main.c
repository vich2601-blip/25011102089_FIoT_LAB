/*
 * CAT 2 - Task 3 : Custom Delay Library (AT89S52)
 * ---------------------------------------------------------------
 * Same 1 Hz blink as Task 2, but the delay now lives in a reusable
 * library (delay.h / delay.c). The application states the time it
 * wants in milliseconds instead of raw loop counts.
 *
 * Files : main.c  (application)
 *         delay.h (library interface)
 *         delay.c (library implementation)
 */
#include <reg52.h>
#include "delay.h"

sbit USER_LED = P1^0;           /* on-board LED, active-low */

#define LED_ON          0
#define LED_OFF         1
#define BLINK_HALF_MS   500     /* 500 ms ON + 500 ms OFF = 1 Hz */

void main(void)
{
    USER_LED = LED_OFF;

    while (1)
    {
        USER_LED = LED_ON;
        delay_ms(BLINK_HALF_MS);

        USER_LED = LED_OFF;
        delay_ms(BLINK_HALF_MS);
    }
}
