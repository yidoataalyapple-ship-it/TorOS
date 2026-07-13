/*
 * uart.c — PL011 UART sürücüsü (QEMU virt 0x09000000)
 */
#include <toros/uart.h>

/* PL011 register offsetleri */
#define UART_DR  0x00
#define UART_FR  0x18
#define UART_IBRD 0x24
#define UART_FBRD 0x28
#define UART_LCR 0x2C
#define UART_CR  0x30

#define FR_RXFE BIT(4)   /* RX FIFO boş */
#define FR_TXFF BIT(5)   /* TX FIFO dolu */
#define FR_BUSY BIT(3)

void uart_init(void)
{
    /*
     * QEMU PL011 varsayılan olarak çalışır durumda gelir;
     * yine de kontrol register'ını garantiye alalım:
     * TXE | RXE | UARTEN
     */
    mmio_write32(UART0_BASE + UART_CR, BIT(0) | BIT(8) | BIT(9));
}

void uart_putc(char c)
{
    while (mmio_read32(UART0_BASE + UART_FR) & FR_TXFF)
        ;
    mmio_write32(UART0_BASE + UART_DR, (u32)(u8)c);
    /* \n -> \r\n dönüşümü terminal uyumu için */
    if (c == '\n')
        uart_putc('\r');
}

void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

void uart_write(const char *buf, size_t len)
{
    for (size_t i = 0; i < len; i++)
        uart_putc(buf[i]);
}

int uart_rx_ready(void)
{
    return !(mmio_read32(UART0_BASE + UART_FR) & FR_RXFE);
}

int uart_getc(void)
{
    if (!uart_rx_ready())
        return -1;
    return (int)(mmio_read32(UART0_BASE + UART_DR) & 0xff);
}
