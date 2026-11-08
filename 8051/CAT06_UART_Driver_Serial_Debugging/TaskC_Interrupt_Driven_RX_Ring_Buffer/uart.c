/*
 * uart.c - interrupt-driven UART driver with an RX ring buffer
 * ----------------------------------------------------------------------
 *  - The serial interrupt (vector 4, address 0x0023) fires on RI or TI.
 *  - RI: the ISR copies SBUF into a 16-byte ring buffer, so no character is
 *        lost even if main() is busy.
 *  - TI: the ISR clears TI and marks the transmitter as free.
 *
 *  Ring buffer:  head = next free slot (written by the ISR)
 *                tail = next byte to read (read by main)
 *                empty when head == tail, full when head + 1 == tail
 */
#include <reg52.h>
#include "uart.h"

#define RX_MASK   (UART_RX_BUF_SIZE - 1)

static volatile unsigned char idata rx_buf[UART_RX_BUF_SIZE];
static volatile unsigned char rx_head = 0;
static volatile unsigned char rx_tail = 0;
static volatile unsigned char rx_lost = 0;
static volatile unsigned char tx_busy = 0;   /* cleared by the ISR, polled by uart_putc() */

void uart_isr(void) interrupt 4 using 2
{
    unsigned char next;

    if (RI)
    {
        RI   = 0;
        next = (rx_head + 1) & RX_MASK;
        if (next != rx_tail)
        {
            rx_buf[rx_head] = SBUF;
            rx_head = next;
        }
        else
        {
            next = SBUF;          /* buffer full: drop the byte */
            rx_lost++;
        }
    }
    if (TI)
    {
        TI      = 0;
        tx_busy = 0;
    }
}

void uart_init(void)
{
    SCON = 0x50;                  /* Mode 1, REN = 1          */
    TMOD = (TMOD & 0x0F) | 0x20;  /* Timer 1 Mode 2           */
    TH1  = 0xFD;                  /* 9600 baud @ 11.0592 MHz  */
    TL1  = 0xFD;
    TR1  = 1;
    tx_busy = 0;
    ES = 1;                       /* enable serial interrupt  */
    EA = 1;                       /* global enable            */
}

void uart_putc(char c)
{
    while (tx_busy);              /* wait for the previous byte */
    tx_busy = 1;
    SBUF = c;
}

void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

bit uart_available(void)
{
    return (rx_head != rx_tail);
}

char uart_getc(void)
{
    char c;
    while (rx_head == rx_tail);   /* wait for data */
    c = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) & RX_MASK;
    return c;
}

unsigned char uart_overflows(void)
{
    return rx_lost;
}
