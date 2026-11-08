/*
 * CAT 5 - Task 3 (optional) : Stepper Motor Speed and Direction Control
 * ----------------------------------------------------------------------
 * Motor  : 28BYJ-48 unipolar stepper + ULN2003 driver board
 * Wiring : P1.0 -> IN1, P1.1 -> IN2, P1.2 -> IN3, P1.3 -> IN4
 *          ULN2003 +5V/GND from the board (common GND).
 *          (P1 has weak internal pull-ups; if the motor is weak add 4.7k
 *           pull-ups from each line to 5 V.)
 *
 * Timer 0 interrupts every 1 ms. The ISR counts milliseconds and outputs the
 * next step of the sequence when the step interval has passed, so the
 * TIMER INTERVAL sets the motor speed:
 *        1000 -> 0100 -> 0010 -> 0001 -> ...   (wave drive)
 *
 * Keypad (rows P2.7..P2.4, columns P2.3..P2.1):
 *   1 = Slow   (100 ms/step)      4 = Reverse direction
 *   2 = Medium ( 50 ms/step)      5 = Stop / Start
 *   3 = Fast   ( 10 ms/step)
 * 7-seg : shows the last key pressed.
 */
#include <reg52.h>
#include "delay.h"

#define STEP_MASK     0x0F               /* motor on P1.0..P1.3 */
#define T0_RELOAD_H   0xFC               /* 1 ms at 11.0592 MHz:              */
#define T0_RELOAD_L   0x72               /* 0xFC66 + 12 cycles ISR latency    */

#define SLOW_MS       100
#define MEDIUM_MS     50
#define FAST_MS       10

unsigned char code STEP_SEQ[4] = { 0x08, 0x04, 0x02, 0x01 };   /* 1000 0100 0010 0001 */
unsigned char code SEG_CODE[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
unsigned char code KEYMAP[3][3] = { {1,2,3}, {4,5,6}, {7,8,9} };

volatile unsigned char step_ms   = SLOW_MS;  /* current speed            */
volatile unsigned char ms_count  = 0;
volatile unsigned char phase     = 0;        /* 0..3 index into STEP_SEQ */
volatile unsigned char clockwise = 1;   /* written by main(), read by the ISR */
volatile unsigned char running   = 1;

void timer0_isr(void) interrupt 1 using 1
{
    TH0 = T0_RELOAD_H;
    TL0 = T0_RELOAD_L;

    if (!running)
        return;

    if (++ms_count >= step_ms)
    {
        ms_count = 0;
        phase = clockwise ? ((phase + 1) & 3) : ((phase - 1) & 3);
        P1 = (P1 & ~STEP_MASK) | STEP_SEQ[phase];
    }
}

/* Blocking keypad read (only used for the user interface - the motor keeps
   stepping in the interrupt while this runs). Returns 1..9 or 0xFF.      */
static unsigned char keypad_get(void)
{
    unsigned char row, col, cols;

    for (row = 0; row < 3; row++)
    {
        P2 = ~(0x80 >> row);
        cols = P2 & 0x0E;
        if (cols != 0x0E)
        {
            delay_ms(20);
            if ((P2 & 0x0E) == cols)
            {
                while ((P2 & 0x0E) != 0x0E);   /* wait for release */
                P2 = 0xFF;
                for (col = 0; col < 3; col++)
                    if (!(cols & (0x08 >> col)))
                        return KEYMAP[row][col];
            }
        }
    }
    P2 = 0xFF;
    return 0xFF;
}

void main(void)
{
    unsigned char key;

    P1 = (P1 & ~STEP_MASK) | STEP_SEQ[0];
    P0 = SEG_CODE[1];

    TMOD = (TMOD & 0xF0) | 0x01;   /* Timer 0, Mode 1, 1 ms */
    TH0 = T0_RELOAD_H;
    TL0 = T0_RELOAD_L;
    ET0 = 1;
    EA  = 1;
    TR0 = 1;

    while (1)
    {
        key = keypad_get();
        if (key == 0xFF)
            continue;

        P0 = SEG_CODE[key];
        switch (key)
        {
            case 1: step_ms = SLOW_MS;   break;
            case 2: step_ms = MEDIUM_MS; break;
            case 3: step_ms = FAST_MS;   break;
            case 4: clockwise = !clockwise; break;
            case 5: running   = !running;   break;
            default: break;
        }
    }
}
