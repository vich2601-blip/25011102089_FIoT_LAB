/*
 * CAT 7 - Task 7C : 4x4 Keypad-Based Character Movement on the 16x2 LCD
 * ----------------------------------------------------------------------
 * Keypad : rows R1..R4 = P2.7..P2.4 (driven LOW one at a time)
 *          cols C1..C4 = P2.3..P2.0 (read, LOW = pressed)
 *          layout  1 2 3 A / 4 5 6 B / 7 8 9 C / * 0 # D
 *
 *   Key 2 : move up        Key 8 : move down
 *   Key 4 : move left      Key 6 : move right
 *   Key 5 : back to home (0,0)   (extra)
 *
 * The '*' character is kept inside rows 0..1 and columns 0..15.
 * Each move erases the old position (writes a space) and draws the
 * character at the new position. One press = one step (waits for release).
 */
#include <reg52.h>
#include "lcd.h"
#include "delay.h"

#define KEY_NONE   0xFF
#define CURSOR_CH  '*'

unsigned char code KEYMAP[4][4] = {
    { '1', '2', '3', 'A' },
    { '4', '5', '6', 'B' },
    { '7', '8', '9', 'C' },
    { '*', '0', '#', 'D' }
};

/* Scan the 4x4 keypad once. Returns the key character or KEY_NONE. */
static unsigned char keypad_scan(void)
{
    unsigned char row, col, cols;

    for (row = 0; row < 4; row++)
    {
        P2 = ~(0x80 >> row);              /* one row LOW, columns HIGH */
        cols = P2 & 0x0F;
        if (cols != 0x0F)
        {
            delay_ms(15);                 /* debounce */
            if ((P2 & 0x0F) == cols)
            {
                for (col = 0; col < 4; col++)
                {
                    if (!(cols & (0x08 >> col)))
                    {
                        P2 = 0xFF;
                        return KEYMAP[row][col];
                    }
                }
            }
        }
    }
    P2 = 0xFF;
    return KEY_NONE;
}

void main(void)
{
    unsigned char row = 0, col = 0;
    unsigned char key;

    lcd_init();
    lcd_goto(row, col);
    lcd_data(CURSOR_CH);

    while (1)
    {
        key = keypad_scan();
        if (key == KEY_NONE)
            continue;

        lcd_goto(row, col);               /* erase the old position */
        lcd_data(' ');

        switch (key)
        {
            case '2': if (row > 0)  row--; break;   /* up    */
            case '8': if (row < 1)  row++; break;   /* down  */
            case '4': if (col > 0)  col--; break;   /* left  */
            case '6': if (col < 15) col++; break;   /* right */
            case '5': row = 0; col = 0;   break;    /* home  */
            default:                      break;
        }

        lcd_goto(row, col);               /* draw at the new position */
        lcd_data(CURSOR_CH);

        while (keypad_scan() != KEY_NONE); /* wait for release */
    }
}
