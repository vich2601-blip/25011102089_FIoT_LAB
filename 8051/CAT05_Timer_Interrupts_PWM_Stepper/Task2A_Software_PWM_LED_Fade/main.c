/*
 * CAT 5 - Task 2A : Software PWM LED Fading driven by a Timer Interrupt
 * ----------------------------------------------------------------------
 * Timer 0, Mode 2 (8-bit auto-reload) interrupts every 100 us:
 *     100 us / 1.085 us = 92 counts  -> TH0 = 256 - 92 = 164 = 0xA4
 *
 * PWM : 10 slots of 100 us = 1 ms period -> 1 kHz (no visible flicker)
 *       duty = number of slots the LED is ON (0..10 -> 0 %..100 %)
 * Fade: every 100 ms the duty changes by one step (10 %):
 *       0 10 20 ... 100 90 80 ... 10 0 %  -> full cycle = 2 s
 *
 * Output : LED on P1.1 (active-HIGH). On the DSO you can see the 1 kHz
 *          PWM with its duty cycle changing.
 * Note   : the ISR is kept very short because it runs every 92 machine
 *          cycles; it uses register bank 1 to avoid pushing R0-R7.
 */
#include <reg52.h>

sbit PWM_LED = P1^1;

#define T0_RELOAD_100US   0xA4
#define PWM_SLOTS         10        /* 10 x 100 us = 1 ms -> 1 kHz         */
#define TICKS_PER_STEP    1000      /* 1000 x 100 us = 100 ms per fade step */

volatile unsigned char duty       = 0;   /* 0..10 */
volatile unsigned char pwm_slot   = 0;
volatile unsigned int  step_ticks = 0;
bit           step_flag  = 0;

void timer0_isr(void) interrupt 1 using 1
{
    /* PWM generation */
    if (++pwm_slot >= PWM_SLOTS)
        pwm_slot = 0;
    PWM_LED = (pwm_slot < duty);

    /* 100 ms time base for the fading */
    if (++step_ticks >= TICKS_PER_STEP)
    {
        step_ticks = 0;
        step_flag  = 1;
    }
}

static void timer0_init_100us(void)
{
    TMOD = (TMOD & 0xF0) | 0x02;        /* Timer 0, Mode 2 auto-reload */
    TH0  = T0_RELOAD_100US;             /* reload value                */
    TL0  = T0_RELOAD_100US;             /* first count                 */
    ET0  = 1;
    EA   = 1;
    TR0  = 1;
}

void main(void)
{
    signed char direction = 1;          /* +1 brighter, -1 dimmer */

    PWM_LED = 0;
    timer0_init_100us();

    while (1)
    {
        if (step_flag)
        {
            step_flag = 0;
            duty += direction;
            if (duty >= PWM_SLOTS) direction = -1;   /* reached 100 % */
            if (duty == 0)         direction = 1;    /* reached 0 %   */
        }
    }
}
