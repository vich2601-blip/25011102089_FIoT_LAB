/*
 * CAT 5 - Task 1 : Timer 0 Periodic Interrupt -> 1 s Software Time Base
 * ----------------------------------------------------------------------
 * Crystal 11.0592 MHz -> machine cycle -> Timer 0 counts -> overflow
 *   -> interrupt (every 1 ms) -> software time base -> LED toggles every 1 s
 *
 *   T_machine = 12 / 11.0592 MHz = 1.085 us
 *   1 ms      = 1 ms / 1.085 us  = 922 counts
 *   preload   = 65536 - 922      = 64614 = 0xFC66
 *   + 12 cycles for the interrupt entry (the timer restarts only after the
 *   ISR reloads it)                       = 0xFC72  -> TH0 = 0xFC, TL0 = 0x72
 *
 * Timer 0 is in Mode 1 (16-bit), so it must be reloaded inside the ISR.
 * The main loop contains NO delay loop: the hardware generates the timing,
 * the software only reacts to the "one second elapsed" event.
 *
 * Output : LED on P1.1 (active-HIGH). Probe P1.1 on the DSO: 0.5 Hz square
 *          wave (1 s HIGH, 1 s LOW).
 */
#include <reg52.h>

sbit LED = P1^1;

#define T0_RELOAD_H   0xFC
#define T0_RELOAD_L   0x72   /* 0xFC66 + 12: the timer restarts only when the ISR
                                   reloads it, so the ~12-cycle interrupt entry is
                                   added back (measured in uCsim: 1.000 s per toggle) */
#define MS_PER_SECOND 1000

volatile unsigned int ms_ticks   = 0;   /* only touched inside the ISR   */
volatile unsigned char second_flag = 0;   /* ISR -> main "1 s passed" event; volatile because
                                           the ISR writes it while main() polls it */

/* Timer 0 overflow interrupt, vector 1 (address 0x000B), register bank 1 */
void timer0_isr(void) interrupt 1 using 1
{
    TH0 = T0_RELOAD_H;                  /* reload for the next 1 ms     */
    TL0 = T0_RELOAD_L;

    if (++ms_ticks >= MS_PER_SECOND)
    {
        ms_ticks    = 0;
        second_flag = 1;
    }
}

static void timer0_init_1ms(void)
{
    TMOD = (TMOD & 0xF0) | 0x01;        /* Timer 0: Mode 1, timer, no gate */
    TH0  = T0_RELOAD_H;
    TL0  = T0_RELOAD_L;
    TF0  = 0;
    ET0  = 1;                           /* enable Timer 0 interrupt        */
    EA   = 1;                           /* global interrupt enable         */
    TR0  = 1;                           /* start Timer 0                   */
}

void main(void)
{
    LED = 0;
    timer0_init_1ms();

    while (1)
    {
        if (second_flag)                /* event from the hardware timer */
        {
            second_flag = 0;
            LED = !LED;
        }
        /* the CPU is free here for other work */
    }
}
