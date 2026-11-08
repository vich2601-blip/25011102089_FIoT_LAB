/*
 * CAT 6 - Task C : Interrupt-Driven UART with an RX Ring Buffer
 * ----------------------------------------------------------------------
 * Uses uart.c (serial ISR + 16-byte ring buffer).
 *
 * Demo:
 *  - every character typed in PuTTY is echoed,
 *  - when Enter is pressed the whole line is reported back with its length:
 *        > hello
 *        Got 5 chars: hello
 *  - the LED on P1.0 toggles in the main loop to show that the CPU is free
 *    while characters arrive in the background (the ISR stores them).
 */
#include <reg52.h>
#include "uart.h"

sbit USER_LED = P1^0;

#define LINE_MAX  32

static void uart_put_uint(unsigned int v)
{
    char digits[5];
    unsigned char n = 0;

    do {
        digits[n++] = '0' + (v % 10);
        v /= 10;
    } while (v);
    while (n)
        uart_putc(digits[--n]);
}

void main(void)
{
    char line[LINE_MAX + 1];
    unsigned char len = 0;
    unsigned int  idle = 0;
    char c;
    unsigned int lost;

    uart_init();
    uart_puts("\r\nCAT6 Task C - interrupt UART + ring buffer\r\n> ");

    while (1)
    {
        if (uart_available())
        {
            c = uart_getc();
            if (c == '\r' || c == '\n')
            {
                if (len == 0)
                    continue;               /* LF after CR (or a blank line): nothing to report */
                line[len] = '\0';
                uart_puts("\r\nGot ");
                uart_put_uint(len);
                uart_puts(" chars: ");
                uart_puts(line);
                lost = uart_overflows();
                if (lost)                        /* report bytes lost to a full RX buffer */
                {
                    uart_puts("  (bytes lost so far: ");
                    uart_put_uint(lost);
                    uart_puts(")");
                }
                uart_puts("\r\n> ");
                len = 0;
            }
            else if (len < LINE_MAX)
            {
                line[len++] = c;
                uart_putc(c);           /* echo */
            }
            else
            {
                uart_putc('\a');       /* line full: bell, character not stored */
            }
        }

        /* background work: heartbeat LED */
        if (++idle == 30000)
        {
            idle = 0;
            USER_LED = !USER_LED;
        }
    }
}
