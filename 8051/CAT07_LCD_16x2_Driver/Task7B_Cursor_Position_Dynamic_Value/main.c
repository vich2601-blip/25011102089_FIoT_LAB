/*
 * CAT 7 - Task 7B : Cursor Positioning and Dynamic Value Update
 * ----------------------------------------------------------------------
 * Uses lcd_goto(row, column), lcd_puts(string), lcd_print_number(value).
 *
 *    +----------------+
 *    |Counter:  137   |   <- only the number field is rewritten
 *    |Up time: 68 s   |   <- every 500 ms
 *    +----------------+
 *
 * Only the changing fields are rewritten (after blanking them), so the
 * static labels are written just once and the display does not flicker.
 */
#include <reg52.h>
#include "lcd.h"
#include "delay.h"

#define COUNT_COL    10
#define UPTIME_COL   9

/* Blank 'width' characters at (row, col), then put the cursor back there */
static void lcd_clear_field(unsigned char row, unsigned char col, unsigned char width)
{
    lcd_goto(row, col);
    while (width--)
        lcd_data(' ');
    lcd_goto(row, col);
}

void main(void)
{
    unsigned int  counter = 0;
    unsigned int  seconds = 0;
    unsigned char half    = 0;

    lcd_init();
    lcd_goto(0, 0);  lcd_puts("Counter:");
    lcd_goto(1, 0);  lcd_puts("Up time:");
    lcd_goto(1, 14); lcd_puts("s");

    while (1)
    {
        lcd_clear_field(0, COUNT_COL, 5);
        lcd_print_number(counter);

        lcd_clear_field(1, UPTIME_COL, 5);
        lcd_print_number(seconds);

        counter++;
        if (++half == 2)
        {
            half = 0;
            seconds++;
        }
        delay_ms(500);
    }
}
