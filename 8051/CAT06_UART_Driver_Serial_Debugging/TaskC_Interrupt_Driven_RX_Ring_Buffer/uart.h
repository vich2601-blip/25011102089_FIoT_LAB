/*
 * uart.h - interrupt-driven UART driver for the AT89S52 (9600-8-N-1)
 */
#ifndef UART_H
#define UART_H

#define UART_RX_BUF_SIZE   16        /* must be a power of 2 */

void          uart_init(void);
void          uart_putc(char c);
void          uart_puts(const char *s);
bit           uart_available(void);  /* 1 if a received byte is waiting */
char          uart_getc(void);       /* blocks until a byte is available */
unsigned char uart_overflows(void);  /* bytes lost because the buffer was full */

#endif
