/*
 * CAT 5 - Task 2B : Variable-Frequency Square-Wave Generator (Timer 0 ISR)
 * ----------------------------------------------------------------------
 * The output pin toggles in the Timer 0 interrupt, so the frequency is set
 * only by the timer reload value (no software delay loop in the signal path).
 *
 * Half period counts N = (1 / 2f) / 1.085 us = 460800 / f
 *   Mode 1 :  100 Hz -> N = 4608 -> reload 0xEE00
 *   Mode 2 :  500 Hz -> N =  922 -> reload 0xFC66
 *   Mode 3 : 1000 Hz -> N =  461 -> reload 0xFE33
 *   Mode 4 : 2000 Hz -> N =  230 -> reload 0xFF1A
 *   Mode 5 : 5000 Hz -> N =   92 -> reload 0xFFA4
 * ISR_LATENCY adds back the cycles lost between the overflow and the
 * reload instruction (tune it while watching the DSO frequency counter).
 *
 * Output : P1.1 -> DSO probe (and LED, which looks half-bright)
 * Button : keypad key, column P2.5 driven LOW, row P2.0 read (as in CAT 3)
 *          each press -> next mode (1..5, then back to 1)
 * 7-seg  : shows the current mode number (common cathode on P0)
 */
#include <reg52.h>
#include "delay.h"

sbit SIGNAL_OUT = P1^1;
sbit KEY_SENSE  = P2^0;
sbit KEY_DRIVE  = P2^5;

#define NUM_MODES     5
#define ISR_LATENCY   12         /* machine cycles (measured in uCsim), fine-tune on the DSO */

/* half-period counts for 100 Hz, 500 Hz, 1 kHz, 2 kHz, 5 kHz */
unsigned int  code HALF_PERIOD[NUM_MODES] = { 4608, 922, 461, 230, 92 };
unsigned char code SEG_CODE[6] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D };

volatile unsigned char reload_h, reload_l;

void timer0_isr(void) interrupt 1 using 1
{
    TH0 = reload_h;
    TL0 = reload_l;
    SIGNAL_OUT = !SIGNAL_OUT;
}

static void set_mode(unsigned char mode)       /* mode = 0..4 */
{
    unsigned int reload = 65536 - HALF_PERIOD[mode] + ISR_LATENCY;

    TR0 = 0;                     /* stop Timer 0 so the change takes effect at once  */
    ET0 = 0;                     /* and update both bytes without the ISR running     */
    reload_h = reload >> 8;
    reload_l = reload & 0xFF;
    TH0 = reload_h;              /* start the new half-period from a clean reload     */
    TL0 = reload_l;
    TF0 = 0;
    ET0 = 1;
    TR0 = 1;

    P0 = SEG_CODE[mode + 1];     /* show 1..5 */
}

static bit key_pressed(void)
{
    if (KEY_SENSE == 0)
    {
        delay_ms(20);
        if (KEY_SENSE == 0)
        {
            while (KEY_SENSE == 0);   /* wait for release: one press = one step.
                                          The signal keeps running in the ISR. */
            delay_ms(20);
            return 1;
        }
    }
    return 0;
}

void main(void)
{
    unsigned char mode = 0;

    KEY_DRIVE = 0;
    KEY_SENSE = 1;

    TMOD = (TMOD & 0xF0) | 0x01;     /* Timer 0, Mode 1 (16-bit) */
    EA  = 1;
    set_mode(mode);                  /* loads TH0/TL0, enables ET0 and starts Timer 0 */

    while (1)
    {
        if (key_pressed())
        {
            mode = (mode + 1) % NUM_MODES;
            set_mode(mode);
        }
    }
}
