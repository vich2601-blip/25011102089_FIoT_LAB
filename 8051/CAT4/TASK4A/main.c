/*
 * CAT 4 : Human-Machine Interface - Matrix Keypad + LEDs + Seven-Segment
 * ----------------------------------------------------------------------
 * Board    : AT89S52 development kit, 11.0592 MHz
 * Keypad   : rows    R1..R4 = P2.7, P2.6, P2.5, P2.4   (driven, one LOW at a time)
 *            columns C1..C3 = P2.3, P2.2, P2.1         (read, LOW = key pressed)
 *            layout   1 2 3 / 4 5 6 / 7 8 9 / * 0 #
 * 7-Segment: common-cathode on Port 0, segment a..g = P0.0..P0.6
 * LEDs     : LED1 P1.1, LED2 P1.3, LED3 P1.5, LED4 P1.7 (active-HIGH)
 *
 *   Key | Display | LED operation
 *   1-4 |   1-4   | LED1..LED4 ON (only that LED)
 *    5  |    5    | Running LED pattern
 *    6  |    6    | Alternate LEDs 1010 <-> 0101
 *    7  |    7    | Binary counter 0..15
 *    8  |    8    | All LEDs blink
 *    9  |    9    | All LEDs OFF
 *    0  |    0    | Reset system
 *
 * The program is split into the functions required by the lab sheet:
 *   keypad_scan(), display_digit(), led_control(),
 *   running_led(), binary_counter(), alternate_led(), blink_all()
 */
#include <reg52.h>
#include "delay.h"

sbit LED1 = P1^1;
sbit LED2 = P1^3;
sbit LED3 = P1^5;
sbit LED4 = P1^7;

#define KEY_NONE      0xFF
#define KEY_STAR      10
#define KEY_HASH      11

#define LOOP_MS       10        /* keypad is scanned every 10 ms           */
#define STEP_LOOPS    25        /* animated patterns step every 250 ms     */
#define DEBOUNCE_MS   15

/* LED operating modes selected by the keys */
#define LEDMODE_STATIC    0     /* LEDs hold a fixed pattern (keys 1-4, 9, 0) */
#define LEDMODE_RUNNING   1
#define LEDMODE_ALTERNATE 2
#define LEDMODE_BINARY    3
#define LEDMODE_BLINK     4

/* Keypad layout: [row][column] */
unsigned char code KEYMAP[4][3] = {
    { 1, 2, 3 },
    { 4, 5, 6 },
    { 7, 8, 9 },
    { KEY_STAR, 0, KEY_HASH }
};

/* Common-cathode segment codes, bit0 = a ... bit6 = g */
unsigned char code SEG_CODE[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66,   /* 0 1 2 3 4 */
    0x6D, 0x7D, 0x07, 0x7F, 0x6F    /* 5 6 7 8 9 */
};

/* ---------------- global state ---------------- */
unsigned char key_value = KEY_NONE;   /* set by keypad_scan() on a NEW press */
unsigned char led_mode  = LEDMODE_STATIC;
unsigned char run_pos   = 0;          /* running_led() position 0..3          */
unsigned char bin_count = 0;          /* binary_counter() value 0..15         */
bit           alt_phase = 0;          /* alternate_led() phase                */
bit           blink_on  = 0;          /* blink_all() phase                    */

/* bit3 -> LED1 ... bit0 -> LED4 */
static void leds_write(unsigned char pattern)
{
    LED1 = (pattern >> 3) & 1;
    LED2 = (pattern >> 2) & 1;
    LED3 = (pattern >> 1) & 1;
    LED4 = pattern & 1;
}

/* Read the three column lines of P2 (returns 0x0E when nothing is pressed) */
static unsigned char read_columns(void)
{
    return P2 & 0x0E;
}

/*
 * keypad_scan()
 * Drives one row LOW at a time and checks the columns. A key is accepted
 * after a 15 ms debounce re-check, and only once per press (the key must be
 * released before it can be reported again). Result goes into key_value.
 */
void keypad_scan(void)
{
    static bit key_held = 0;
    unsigned char row, col, cols;
    bit found = 0;

    key_value = KEY_NONE;

    for (row = 0; row < 4 && !found; row++)
    {
        P2 = ~(0x80 >> row);            /* selected row LOW, all others HIGH,
                                           column lines written HIGH (inputs) */
        cols = read_columns();
        if (cols != 0x0E)
        {
            delay_ms(DEBOUNCE_MS);
            if (read_columns() == cols)  /* still the same after 15 ms */
            {
                for (col = 0; col < 3; col++)
                {
                    if (!(cols & (0x08 >> col)))   /* C1=P2.3, C2=P2.2, C3=P2.1 */
                    {
                        found = 1;
                        if (!key_held)
                            key_value = KEYMAP[row][col];
                        break;
                    }
                }
            }
        }
    }
    P2 = 0xFF;                          /* release all rows */
    key_held = found;
}

/* Show one decimal digit on the seven-segment display (blank if > 9) */
void display_digit(unsigned char value)
{
    P0 = (value <= 9) ? SEG_CODE[value] : 0x00;
}

/* Animated LED functions - each call advances the pattern by one step */
void running_led(void)
{
    leds_write(0x08 >> run_pos);        /* 1000, 0100, 0010, 0001 */
    run_pos = (run_pos + 1) & 0x03;
}

void binary_counter(void)
{
    leds_write(bin_count);
    bin_count = (bin_count + 1) & 0x0F;
}

void alternate_led(void)
{
    leds_write(alt_phase ? 0x05 : 0x0A); /* 1010 <-> 0101 */
    alt_phase = !alt_phase;
}

void blink_all(void)
{
    leds_write(blink_on ? 0x00 : 0x0F);
    blink_on = !blink_on;
}

/* Decide what the LEDs do for the key that was pressed */
void led_control(unsigned char key)
{
    switch (key)
    {
        case 1: case 2: case 3: case 4:
            led_mode = LEDMODE_STATIC;
            leds_write(0x08 >> (key - 1));      /* only LEDn ON */
            break;
        case 5:
            led_mode = LEDMODE_RUNNING;   run_pos = 0;     running_led();  break;
        case 6:
            led_mode = LEDMODE_ALTERNATE; alt_phase = 0;   alternate_led(); break;
        case 7:
            led_mode = LEDMODE_BINARY;    bin_count = 0;   binary_counter(); break;
        case 8:
            led_mode = LEDMODE_BLINK;     blink_on = 0;    blink_all();    break;
        case 9:
            led_mode = LEDMODE_STATIC;    leds_write(0x00); break;
        case 0:                                         /* reset system */
            led_mode  = LEDMODE_STATIC;
            run_pos   = 0;
            bin_count = 0;
            alt_phase = 0;
            blink_on  = 0;
            leds_write(0x00);
            break;
        default:                                        /* '*' and '#': unused */
            break;
    }
}

void main(void)
{
    unsigned char loops = 0;

    P2 = 0xFF;
    leds_write(0x00);
    display_digit(KEY_NONE);            /* blank display at power-on */

    while (1)
    {
        keypad_scan();
        if (key_value <= 9)
        {
            display_digit(key_value);   /* 1. show the key            */
            led_control(key_value);     /* 2. run its LED function    */
            loops = 0;
        }

        if (++loops >= STEP_LOOPS)      /* animate every 250 ms */
        {
            loops = 0;
            switch (led_mode)
            {
                case LEDMODE_RUNNING:   running_led();    break;
                case LEDMODE_ALTERNATE: alternate_led();  break;
                case LEDMODE_BINARY:    binary_counter(); break;
                case LEDMODE_BLINK:     blink_all();      break;
                default:                                  break;
            }
        }

        delay_ms(LOOP_MS);
    }
}
