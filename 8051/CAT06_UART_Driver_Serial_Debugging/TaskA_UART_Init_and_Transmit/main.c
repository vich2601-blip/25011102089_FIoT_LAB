/*
 * CAT 6 - Task A : UART Initialisation and Character Transmission (9600-8-N-1)
 * ----------------------------------------------------------------------
 * Pins   : P3.1 TXD -> USB-TTL RX,  P3.0 RXD <- USB-TTL TX,  common GND
 * PC     : PuTTY, Serial, COMx, 9600 baud, 8 data bits, no parity, 1 stop bit
 *
 * Stage answers (lab sheet hints)
 *  - SCON selects the serial mode. Mode 1 (SM0=0, SM1=1) = 8-bit UART with a
 *    variable baud rate; REN=1 enables reception  -> SCON = 0101 0000 = 0x50
 *  - SBUF is written to transmit and read to receive (two physical registers,
 *    one address).
 *  - TI is set by hardware when the stop bit has been sent; software clears it.
 *  - Timer 1 in Mode 2 (8-bit auto-reload) generates the baud rate:
 *        baud = f_osc / (12 x 32 x (256 - TH1))         (SMOD = 0)
 *        256 - TH1 = 11059200 / (384 x 9600) = 3  ->  TH1 = 253 = 0xFD (exact)
 *
 * Program: first sends one known character 'U' (Stage 8 check, 0x55 is a nice
 * 01010101 pattern on the scope), then "Hello from 8051" continuously.
 */
#include <reg52.h>
#include "delay.h"

void uart_init(void)
{
    SCON = 0x50;                    /* Mode 1, REN = 1                */
    TMOD = (TMOD & 0x0F) | 0x20;    /* Timer 1, Mode 2 (auto-reload)  */
    TH1  = 0xFD;                    /* 9600 baud @ 11.0592 MHz        */
    TL1  = 0xFD;
    PCON &= 0x7F;                   /* SMOD = 0                       */
    TR1  = 1;                       /* start the baud-rate generator  */
}

void uart_putc(char c)
{
    SBUF = c;                       /* start transmission             */
    while (!TI);                    /* wait until the frame is sent   */
    TI = 0;                         /* clear the flag for next time   */
}

void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

void main(void)
{
    uart_init();

    uart_putc('U');                 /* Stage 8: one known character   */
    uart_puts("\r\n");

    while (1)
    {
        uart_puts("Hello from 8051\r\n");
        delay_ms(500);
    }
}
