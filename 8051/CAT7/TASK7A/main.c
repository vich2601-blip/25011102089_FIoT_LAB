/*
 * CAT 7 - Task 7A : LCD Initialisation and Character / String Display
 * ----------------------------------------------------------------------
 * 1. lcd_init() puts the HD44780 in 4-bit, 2-line mode.
 * 2. Set DDRAM address 0x80 (row 0, column 0) and send 'A' as DATA (RS = 1).
 * 3. Show a string on the rest of row 0 and another on row 1.
 *
 *    +----------------+
 *    |A CAT7 LCD TEST |
 *    |8051 4-BIT MODE |
 *    +----------------+
 */
#include <reg52.h>
#include "lcd.h"

void main(void)
{
    lcd_init();

    lcd_cmd(LCD_SET_DDRAM | 0x00);   /* 0x80: first position of the first row */
    lcd_data('A');                   /* single character                      */

    lcd_goto(0, 2);
    lcd_puts("CAT7 LCD TEST");       /* string on row 0                       */

    lcd_goto(1, 0);
    lcd_puts("8051 4-BIT MODE");     /* string on row 1                       */

    while (1)
    {
        /* nothing else to do - the LCD keeps showing the text */
    }
}
