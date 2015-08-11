#include <test/test_vtimer.h>
#include <log/uart_print.h>
#include <guestloader.h>
#include <gic.h>
#include "timer.h"

#define BOOT_COUNT  5
#define SHED_TICK 1000
#define BOOT_COUNT_RATIO  (SHED_TICK/BOOT_COUNT)
#define VTIMER_IRQ 30

int bootcount = BOOT_COUNT * BOOT_COUNT_RATIO;
/**
 *  * @brief Handler of timer interrupt.
 *   */
void timer_handler(int irq, void *pregs, void *pdata)
{
    if (0 == (bootcount % BOOT_COUNT_RATIO)) {
        uart_print("Hit any key to stop autoboot : ");
        uart_print_hex32(bootcount / BOOT_COUNT_RATIO);
        uart_print("\n");
    }
    if (bootcount == 0)
        guestloader_flag_autoboot(1);
    bootcount--;
}

static volatile unsigned int *irqEnable1 = (unsigned int *) (0x3f00b210);
static volatile unsigned int *irqEnable2 = (unsigned int *) (0x3f00b214);
static volatile unsigned int *irqEnableBasic = (unsigned int *) (0x3f00b218);
static volatile unsigned int *armTimerLoad = (unsigned int *) (0x3f00b400);
static volatile unsigned int *armTimerValue = (unsigned int *) (0x3f00b404);
static volatile unsigned int *armTimerControl = (unsigned int *) (0x3f00b408);
static volatile unsigned int *armTimerIRQClear = (unsigned int *) (0x3f00b40c);

static void enable_irqs1(void) {
//	uint32_t cpsr = cpsr_read();
//	cpsr &= ~CPSR_IRQ_DISABLE;
//	cpsr_write_c(cpsr);
	unsigned long temp;
	__asm__ __volatile__("mrs %0, cpsr\n"
			     "bic %0, %0, #0x80\n"
			     "msr cpsr_c, %0"
			     : "=r" (temp)
			     :
			     : "memory");

}

void timer_init(void)
{
//    /* Registers vtimer hander */
//    gic_set_irq_handler(VTIMER_IRQ, timer_handler, 0);
//    /* Enables receiving virtual timer interrupt */
//    timer_enable();

	enable_irqs1();

	/* Use the ARM timer - BCM 2832 peripherals doc, p.196 */
	/* Enable ARM timer IRQ */
	*irqEnableBasic = 0x00000001;
	/* Interrupt every 1024 * 256 (prescaler) timer ticks */
	*armTimerLoad = 0x00000400;
	/* Timer enabled, interrupt enabled, prescale=256, "23 bit" counter
	 * (did they mean 32 bit?)
	 */
	*armTimerControl = 0x000000aa;
}

void timer_disable(void)
{
    /* Disables receiving virtual timer interrupt. */
    vtimer_mask(1);
}

void timer_enable(void)
{
    /* Enables receiving virtual timer interrupt. */
    vtimer_mask(0);
}

