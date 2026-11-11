/*
 * CAT 5 - Task 2C : Different Signal Types from a Timer-Interrupt Bit Player
 * ----------------------------------------------------------------------
 * Every waveform is stored as a string of '1' and '0'. A 100 us Timer 0
 * interrupt (Mode 2 auto-reload, TH0 = 0xA4) plays the pattern one bit at a
 * time; each bit is held for BIT_TICKS interrupts.
 *
 *   1  Square wave           101010101010
 *   2  Variable duty cycle   1111000011110000   (change the 1/0 ratio to change duty)
 *   3  Pulse train           1000000000          (one short pulse, long gap)
 *   4  Burst signal          1010101010 0000000000
 *
 * With BIT_TICKS = 5 one bit = 0.5 ms, so the square wave is 1 kHz.
 * Output : P1.1 -> DSO.   Button (P2.5 low / P2.0 read) -> next pattern.
 * 7-seg  : shows the pattern number.
 */
#include <reg52.h>
#include "delay.h"

sbit SIGNAL_OUT = P1^1;
sbit KEY_SENSE  = P2^0;
sbit KEY_DRIVE  = P2^5;

#define T0_RELOAD_100US  0xA4
#define BIT_TICKS        5        /* 5 x 100 us = 0.5 ms per bit */
#define NUM_PATTERNS     4

unsigned char code PAT_SQUARE[] = "10";
unsigned char code PAT_DUTY[]   = "11110000";
unsigned char code PAT_PULSE[]  = "1000000000";
unsigned char code PAT_BURST[]  = "10101010100000000000";

unsigned char code * code PATTERNS[NUM_PATTERNS] = { PAT_SQUARE, PAT_DUTY, PAT_PULSE, PAT_BURST };
unsigned char code PATTERN_LEN[NUM_PATTERNS]    = { 2, 8, 10, 20 };
unsigned char code SEG_CODE[5] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66 };

unsigned char code *cur_pattern = PAT_SQUARE;   /* changed only while ET0 = 0 */
volatile unsigned char cur_len  = 2;
volatile unsigned char bit_idx  = 0;
volatile unsigned char tick_cnt = 0;

void timer0_isr(void) interrupt 1 using 1
{
    if (++tick_cnt >= BIT_TICKS)
    {
        tick_cnt = 0;
        SIGNAL_OUT = (cur_pattern[bit_idx] == '1');
        if (++bit_idx >= cur_len)
            bit_idx = 0;
    }
}

static void select_pattern(unsigned char n)       /* n = 0..3 */
{
    ET0 = 0;
    cur_pattern = PATTERNS[n];
    cur_len     = PATTERN_LEN[n];
    bit_idx     = 0;
    tick_cnt    = 0;
    ET0 = 1;
    P0 = SEG_CODE[n + 1];
}

static bit key_pressed(void)
{
    if (KEY_SENSE == 0)
    {
        delay_ms(20);
        if (KEY_SENSE == 0)
        {
            while (KEY_SENSE == 0);
            delay_ms(20);
            return 1;
        }
    }
    return 0;
}

void main(void)
{
    unsigned char n = 0;

    KEY_DRIVE = 0;
    KEY_SENSE = 1;

    TMOD = (TMOD & 0xF0) | 0x02;      /* Timer 0, Mode 2 */
    TH0 = T0_RELOAD_100US;
    TL0 = T0_RELOAD_100US;
    select_pattern(n);
    EA  = 1;
    TR0 = 1;

    while (1)
    {
        if (key_pressed())
        {
            n = (n + 1) % NUM_PATTERNS;
            select_pattern(n);
        }
    }
}
