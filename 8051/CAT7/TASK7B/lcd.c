/*
 * lcd.c - HD44780 16x2 character LCD driver, 4-bit mode (AT89S52)
 * See lcd.h for wiring and command values.
 */
#include <reg52.h>
#include "lcd.h"
#include "delay.h"

sbit LCD_RS = P3^2;     /* 0 = command, 1 = data            */
sbit LCD_E  = P3^3;     /* data is latched on the falling edge */
sbit LCD_D4 = P3^4;
sbit LCD_D5 = P3^5;
sbit LCD_D6 = P3^6;
sbit LCD_D7 = P3^7;

/* Put the low 4 bits of 'nibble' on D7..D4 and pulse E */
static void lcd_write_nibble(unsigned char nibble)
{
    LCD_D4 = (nibble >> 0) & 1;
    LCD_D5 = (nibble >> 1) & 1;
    LCD_D6 = (nibble >> 2) & 1;
    LCD_D7 = (nibble >> 3) & 1;

    LCD_E = 1;          /* E pulse width > 450 ns: a few instructions is enough */
    LCD_E = 1;
    LCD_E = 0;          /* falling edge latches the nibble */
}

/* A byte is sent as two nibbles: high nibble first, then low nibble */
static void lcd_write_byte(unsigned char value)
{
    lcd_write_nibble(value >> 4);
    lcd_write_nibble(value & 0x0F);
}

void lcd_cmd(unsigned char cmd)
{
    LCD_RS = 0;
    lcd_write_byte(cmd);
    if (cmd == LCD_CLEAR || cmd == LCD_HOME)
        delay_ms(2);        /* these two need 1.52 ms */
    else
        delay_50us(1);      /* all others need 37 us  */
}

void lcd_data(unsigned char ch)
{
    LCD_RS = 1;
    lcd_write_byte(ch);
    delay_50us(1);
}

/* Datasheet "initialising by instruction" sequence for 4-bit mode */
void lcd_init(void)
{
    LCD_E  = 0;
    LCD_RS = 0;
    delay_ms(20);               /* > 15 ms after VCC rises to 4.5 V */

    lcd_write_nibble(0x03);     /* function set (8-bit) #1 */
    delay_ms(5);                /* > 4.1 ms                */
    lcd_write_nibble(0x03);     /* #2                      */
    delay_ms(1);                /* > 100 us                */
    lcd_write_nibble(0x03);     /* #3                      */
    delay_ms(1);
    lcd_write_nibble(0x02);     /* switch to 4-bit mode    */
    delay_ms(1);

    lcd_cmd(LCD_FUNC_4BIT_2L);  /* 4-bit, 2 lines, 5x8     */
    lcd_cmd(LCD_DISPLAY_ON);    /* display on, cursor off  */
    lcd_cmd(LCD_ENTRY_INC);     /* cursor moves right      */
    lcd_cmd(LCD_CLEAR);
}

void lcd_clear(void)
{
    lcd_cmd(LCD_CLEAR);
}

/* row 0..1, column 0..15 */
void lcd_goto(unsigned char row, unsigned char column)
{
    unsigned char addr = (row ? LCD_ROW1_ADDR : 0x00) + (column & 0x0F);
    lcd_cmd(LCD_SET_DDRAM | addr);
}

void lcd_puts(const char *s)
{
    while (*s)
        lcd_data(*s++);
}

/* Print an unsigned number in decimal (0..65535) */
void lcd_print_number(unsigned int value)
{
    char digits[5];
    unsigned char n = 0;

    do {
        digits[n++] = '0' + (value % 10);   /* least significant digit first */
        value /= 10;
    } while (value);

    while (n)
        lcd_data(digits[--n]);              /* print most significant first */
}
