/*
 * CAT 3 : Four-State LED Pattern Generator with a Debounced Push-Button
 * ----------------------------------------------------------------------
 * Board   : AT89S52 development kit, 11.0592 MHz
 * LEDs    : LED1 P1.1, LED2 P1.3, LED3 P1.5, LED4 P1.7 (active-HIGH,
 *           anode to the pin through a resistor, cathode to GND)
 * Button  : one key of the on-board keypad used as a push-button:
 *           column line P2.5 is driven LOW, row line P2.0 is read
 *           (P2.0 = 0 while the key is held)
 *
 * State machine (one valid press = exactly one transition):
 *   MODE_IDLE(0) -> MODE_RUNNING(1) -> MODE_BINARY(2) -> MODE_ALTERNATE(3)
 *   -> MODE_FLASH(4) -> back to MODE_RUNNING(1) ...
 *
 * Pattern notation is LED1 LED2 LED3 LED4  (LED1 = bit 3, LED4 = bit 0)
 *   Mode 1  Running   : 1000 -> 0100 -> 0010 -> 0001 -> ...
 *   Mode 2  Binary    : 0000 -> 0001 -> ... -> 1111 -> 0000
 *   Mode 3  Alternate : 1010 <-> 0101
 *   Mode 4  Flash     : 1111 <-> 0000
 *
 * Design notes
 *  - The main loop runs every 10 ms, so the button is checked 100 times a
 *    second even while a pattern is running (no long blocking delays).
 *  - Debouncing: a change in the key level is accepted only if it is still
 *    there 20 ms later.
 *  - Edge detection: the mode only changes on the RELEASED -> PRESSED edge,
 *    so holding the key down does not keep changing the mode.
 */
#include <reg52.h>
#include "delay.h"

/* ---------------- pin map ---------------- */
sbit KEY_SENSE = P2^0;          /* row line, read                    */
sbit KEY_DRIVE = P2^5;          /* column line, held low             */

sbit LED1 = P1^1;
sbit LED2 = P1^3;
sbit LED3 = P1^5;
sbit LED4 = P1^7;

/* ---------------- constants ---------------- */
#define MODE_IDLE        0
#define MODE_RUNNING     1
#define MODE_BINARY      2
#define MODE_ALTERNATE   3
#define MODE_FLASH       4
#define LAST_MODE        MODE_FLASH

#define LOOP_MS          10     /* main loop period                  */
#define STEP_LOOPS       30     /* 30 x 10 ms = 300 ms per pattern step */
#define DEBOUNCE_MS      20

/* Write a 4-bit pattern to the LEDs: bit3 -> LED1 ... bit0 -> LED4 */
static void leds_write(unsigned char pattern)
{
    LED1 = (pattern >> 3) & 1;
    LED2 = (pattern >> 2) & 1;
    LED3 = (pattern >> 1) & 1;
    LED4 = pattern & 1;
}

/* Returns 1 exactly once per physical press (debounced press edge) */
static bit button_pressed_event(void)
{
    static bit stable_pressed = 0;      /* last accepted (debounced) level */
    bit raw_pressed = (KEY_SENSE == 0);

    if (raw_pressed != stable_pressed)
    {
        delay_ms(DEBOUNCE_MS);           /* let the contacts settle       */
        if ((KEY_SENSE == 0) == raw_pressed)
        {
            stable_pressed = raw_pressed;
            if (stable_pressed)
                return 1;               /* new press accepted            */
        }
    }
    return 0;
}

/* Compute the LED pattern for the given mode and step number */
static unsigned char pattern_for(unsigned char mode, unsigned char step)
{
    switch (mode)
    {
        case MODE_RUNNING:   return 0x08 >> (step & 0x03);          /* 1000,0100,0010,0001 */
        case MODE_BINARY:    return step & 0x0F;                     /* 0..15               */
        case MODE_ALTERNATE: return (step & 1) ? 0x05 : 0x0A;        /* 1010 / 0101         */
        case MODE_FLASH:     return (step & 1) ? 0x00 : 0x0F;        /* 1111 / 0000         */
        default:             return 0x00;                            /* idle: all OFF       */
    }
}

void main(void)
{
    unsigned char mode       = MODE_IDLE;
    unsigned char step       = 0;
    unsigned char loop_count = 0;

    KEY_DRIVE = 0;              /* select the key's column            */
    KEY_SENSE = 1;              /* write 1 so the pin can be read     */
    leds_write(0x00);           /* power-on: all LEDs OFF             */

    while (1)
    {
        /* 1. Button -> next state */
        if (button_pressed_event())
        {
            mode = (mode >= LAST_MODE) ? MODE_RUNNING : mode + 1;
            step = 0;
            loop_count = STEP_LOOPS;        /* show the new pattern at once */
        }

        /* 2. Pattern engine: advance one step every 300 ms */
        if (++loop_count >= STEP_LOOPS)
        {
            loop_count = 0;
            leds_write(pattern_for(mode, step));
            step++;
            if (mode == MODE_RUNNING && step >= 4)  step = 0;
            if (mode == MODE_BINARY  && step >= 16) step = 0;
        }

        delay_ms(LOOP_MS);
    }
}
