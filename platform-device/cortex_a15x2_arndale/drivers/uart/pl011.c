#include "pl011.h"

#include <include/asm_io.h>
#include <k-hypervisor-config.h>

#define RPI_URART_BASE 0x3F201000
#define UART0_DR 0x00
#define UART0_FR 0x18
#define FR_TXFF  (1 << 5)
#define FR_RXFE (1 << 4)
//enum {
//	FR_TXFF = 1 << 5, // Transmit FIFO full
//	FR_RXFE = 1 << 4, // Receive FIFO empty
//};
void putc(char c) {
// wait for space in the transmit FIFO
	while (*(volatile uint32_t *)(RPI_URART_BASE + UART0_FR) & FR_TXFF) {
	}
// add char to transmit FIFO
	*(volatile uint32_t *)(RPI_URART_BASE + UART0_DR) = c;
}
char getc(void) {
// wait for data in the receive FIFO
	while (*(volatile uint32_t *)(RPI_URART_BASE+UART0_FR) & FR_RXFE) {
	}
// extract char from receive FIFO
	return *(volatile uint32_t *)(RPI_URART_BASE + UART0_DR);
}
void puts(const char *str) {
// putc until 0 byte
	while (*str)
		putc(*str++);
}
void put_uint32(uint32_t x) {
	static const char HEX[] = "0123456789ABCDEF";
	int i = 0;
	putc('0');
	putc('x');
	for (i = 28; i >= 0; i -= 4) {
		putc(HEX[(x >> i) % 16]);
	}
}
void show(const char *str, uint32_t x) {
	puts(str);
	puts(": ");
	put_uint32(x);
	putc('\n');
}



void uart_putc(const char c)
{
	putc(c);
}
void uart_print(const char *str)
{
    while (*str)
        uart_putc(*str++);
}

void uart_print_hex32(uint32_t v)
{
	put_uint32(v);
}

void uart_print_hex64(uint64_t v)
{
    uart_print_hex32(v >> 32);
    uart_print_hex32((uint32_t)(v & 0xFFFFFFFF));
}

