/*
 * torOS PL011 UART Driver
 * ARM64 QEMU virt machine
 */

#include "../include/toros.h"

/* QEMU virt PL011 UART */
#define UART_BASE       0x09000000
#define UART_CLK        24000000
#define UART_BAUD       115200

/* PL011 registers */
#define UART_DR         0x00    /* Data Register */
#define UART_RSR        0x04    /* Receive Status */
#define UART_FR         0x18    /* Flag Register */
#define UART_IBRD       0x24    /* Integer Baud Rate */
#define UART_FBRD       0x28    /* Fractional Baud Rate */
#define UART_LCRH       0x2C    /* Line Control */
#define UART_CR         0x30    /* Control Register */
#define UART_IMSC       0x38    /* Interrupt Mask */
#define UART_ICR        0x44    /* Interrupt Clear */

/* Flag Register bits */
#define FR_RXFE         0x10    /* Receive FIFO empty */
#define FR_TXFF         0x20    /* Transmit FIFO full */
#define FR_TXFE         0x80    /* Transmit FIFO empty */

static volatile uint32 *uart = (volatile uint32 *)UART_BASE;

static inline void reg_write(uint reg, uint32 val)
{
    uart[reg >> 2] = val;
}

static inline uint32 reg_read(uint reg)
{
    return uart[reg >> 2];
}

void uart_init(void)
{
    /* Disable UART */
    reg_write(UART_CR, 0);

    /* Clear interrupts */
    reg_write(UART_ICR, 0x7FF);

    /* Set baud rate: divisor = UART_CLK / (16 * BAUD) */
    uint32 ibrd = UART_CLK / (16 * UART_BAUD);
    uint32 fbrd = ((UART_CLK % (16 * UART_BAUD)) * 4 + UART_BAUD / 2) / UART_BAUD;
    reg_write(UART_IBRD, ibrd);
    reg_write(UART_FBRD, fbrd);

    /* 8 bits, no parity, 1 stop, FIFO enable */
    reg_write(UART_LCRH, 0x70);

    /* Enable UART, TX, RX */
    reg_write(UART_CR, 0x301);
}

void uart_putc(char c)
{
    /* Wait until TX FIFO not full */
    while (reg_read(UART_FR) & FR_TXFF)
        ;
    reg_write(UART_DR, c);
}

int uart_getc(char *c)
{
    if (reg_read(UART_FR) & FR_RXFE)
        return 0;
    *c = (char)reg_read(UART_DR);
    return 1;
}

void uart_puts(const char *s)
{
    while (*s) {
        if (*s == '\n')
            uart_putc('\r');
        uart_putc(*s++);
    }
}
