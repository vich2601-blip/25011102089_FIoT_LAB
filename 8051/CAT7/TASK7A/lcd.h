/*
 * lcd.h - HD44780 16x2 character LCD driver, 4-bit mode (AT89S52)
 * ----------------------------------------------------------------------
 * Wiring : RS -> P3.2, E -> P3.3, D4..D7 -> P3.4..P3.7, RW -> GND (write only)
 *
 * Commands used (from the HD44780 datasheet instruction table)
 *   0x01  Clear display              (1.52 ms)
 *   0x02  Return home                (1.52 ms)
 *   0x06  Entry mode: increment, no shift
 *   0x0C  Display ON, cursor OFF, blink OFF
 *   0x0E  Display ON, cursor ON
 *   0x28  Function set: 4-bit, 2 lines, 5x8 font
 *   0x18 / 0x1C  Shift display left / right
 *   0x80 + addr  Set DDRAM address  (row 0 = 0x00-0x0F, row 1 = 0x40-0x4F)
 */
#ifndef LCD_H
#define LCD_H

#define LCD_CLEAR         0x01
#define LCD_HOME          0x02
#define LCD_ENTRY_INC     0x06
#define LCD_DISPLAY_ON    0x0C
#define LCD_CURSOR_ON     0x0E
#define LCD_FUNC_4BIT_2L  0x28
#define LCD_SHIFT_LEFT    0x18
#define LCD_SHIFT_RIGHT   0x1C
#define LCD_SET_DDRAM     0x80
#define LCD_ROW1_ADDR     0x40

void lcd_init(void);
void lcd_cmd(unsigned char cmd);
void lcd_data(unsigned char ch);
void lcd_clear(void);
void lcd_goto(unsigned char row, unsigned char column);
void lcd_puts(const char *s);
void lcd_print_number(unsigned int value);

#endif
