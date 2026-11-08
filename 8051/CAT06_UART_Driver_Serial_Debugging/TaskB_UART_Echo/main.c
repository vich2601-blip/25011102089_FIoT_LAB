/*
 * CAT 6 - Task B : UART Echo  (PC -> 8051 -> PC)
 * ----------------------------------------------------------------------
 * Whatever is typed in PuTTY is received by the 8051 and sent straight back.
 *   Type "ABC123"     -> terminal shows ABC123
 *   Type "Hello 8051" -> terminal shows Hello 8051
 *
 * Receive: RI is set by hardware when a full character has arrived in SBUF.
 *          Software reads SBUF and clears RI.
 * Enter (CR) is expanded to CR+LF so each line starts on a new row.
 */
#include <reg52.h>

void uart_init(void)
{
    SCON = 0x50;                    /* Mode 1, receiver enabled       */
    TMOD = (TMOD & 0x0F) | 0x20;    /* Timer 1 Mode 2                 */
    TH1  = 0xFD;                    /* 9600 baud                      */
    TL1  = 0xFD;
    TR1  = 1;
}

void uart_putc(char c)
{
    SBUF = c;
    while (!TI);
    TI = 0;
}

char uart_getc(void)
{
    char c;
    while (!RI);                    /* wait for a character           */
    c  = SBUF;                      /* received byte                  */
    RI = 0;
    return c;
}

void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

void main(void)
{
    char rx;

    uart_init();
    uart_puts("8051 UART echo ready - type something:\r\n");

    while (1)
    {
        rx = uart_getc();
        uart_putc(rx);              /* echo */
        if (rx == '\r')
            uart_putc('\n');
    }
}
