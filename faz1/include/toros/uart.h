/*
 * uart.h — PL011 UART sürücüsü (QEMU virt, 0x09000000)
 * (Planda "MEVCUT" sayılan temel bileşen)
 */
#ifndef TOROS_UART_H
#define TOROS_UART_H

#include <toros/types.h>

#define UART0_BASE 0x09000000UL

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
int  uart_rx_ready(void);   /* 1 = okunacak karakter var */
int  uart_getc(void);       /* -1 = boş */
void uart_write(const char *buf, size_t len);

#endif
