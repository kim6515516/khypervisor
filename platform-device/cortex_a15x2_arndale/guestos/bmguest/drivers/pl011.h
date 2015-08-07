#ifndef __ASM_RPI2_UART_H_
#define __ASM_RPI2_UART_H_

#include "arch_types.h"
typedef unsigned int uint32_t;
void putc(char c);
char getc(void);
void puts(const char *str);
void put_uint32(uint32_t x);
void show(const char *str, uint32_t x);

void uart_putc(const char c);
void uart_print(const char *str);

void uart_print_hex32(uint32_t v);

void uart_print_hex64(uint64_t v);
#endif
