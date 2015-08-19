/*
 * Copyright (c) 2012 Linaro Limited
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Linaro Limited nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 */

/* This file just contains a small glue function which fishes the
 * location of kernel etc out of linker script defined symbols, and
 * calls semi_loader functions to do the actual work of loading
 * and booting the kernel.
 */

#include <asm-arm_inline.h>
#include <log/uart_print.h>
#include <gic.h>
#include <test/tests.h>
#include <test/test_vtimer.h>
#ifdef _SMP_
#include <smp.h>
#endif
#ifdef _CPUISOLATED_
#include <smp.h>
#endif
/* #define TESTS_ENABLE_VDEV_SAMPLE */

#ifdef __MONITOR_CALL_HVC__
#define hsvc_ping()     asm("hvc #0xFFFE")
#define hsvc_yield()    asm("hvc #0xFFFD")
#else
#define SWITCH_MANUAL() asm("smc #0")
#endif

#ifndef GUEST_LABEL
#define GUEST_LABEL "[guest] :"
#endif

#ifndef NUM_ITERATIONS
#define NUM_ITERATIONS 10
#endif



static volatile unsigned int *irqEnable1 = (unsigned int *) (0x3f00b210);
static volatile unsigned int *irqEnable2 = (unsigned int *) (0x3f00b214);
static volatile unsigned int *irqEnableBasic = (unsigned int *) (0x3f00b218);
static volatile unsigned int *armTimerLoad = (unsigned int *) (0x3f00b400);
static volatile unsigned int *armTimerValue = (unsigned int *) (0x3f00b404);
static volatile unsigned int *armTimerControl = (unsigned int *) (0x3f00b408);
static volatile unsigned int *armTimerIRQClear = (unsigned int *) (0x3f00b40c);
static volatile unsigned int *armTimerReload = (unsigned int *) (0x3f00b418);
static volatile unsigned int *arm_timer_div = (unsigned int *) (0x3f00b41C);
#define SYS_FREQ 0x1000000

void enable_irqs(void) {
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
void enable_fiqs(void) {
	unsigned long temp;
	__asm__ __volatile__(					\
	"mrs	%0, cpsr		@ stf\n"		\
"	bic	%0, %0, #64\n"					\
"	msr	cpsr_c, %0"					\
	: "=r" (temp)						\
	:							\
	: "memory");

}

#define CS      0x3F003000
#define CLO     0x3F003004
#define C0      0x3F00300C
#define C1      0x3F003010
#define C2      0x3F003014
#define C3      0x3F003018

#define INTERVAL 0x10000000


#define ENABLE_TIMER_IRQ() PUT32(CS,0x8)
#define DISABLE_TIMER_IRQ() PUT32(CS,~2);

void
set_tick_and_enable_timer()
{
  unsigned int rx = GET32(CLO);
  rx += INTERVAL;
  PUT32(C1,rx);

  ENABLE_TIMER_IRQ();
}


/*
 * Start_hw
 */
void init_hw()
{
    unsigned int ra;
    unsigned int rx;

    /* Make gpio pin tied to the led an output */

    //led off


    /* Set up delay before timer interrupt (we use CM1) */
    rx=GET32(CLO);
    rx += INTERVAL;
    PUT32(C3,rx);

    /* Enable interrupt *line* */
    PUT32(0x3F00B210, 0x00000001);
    PUT32(0x3F00B210, 0x00000001);

//    PUT32(0x3F00B210, 0x00000002);
//    PUT32(0x3F00B210, 0x00000004);
//    PUT32(0x3F00B210, 0x00000008);

    /* Enable irq triggering by the *system timer* peripheral */
    /*   - we use the compare module CM1 */
    ENABLE_TIMER_IRQ();


}

void timer_test(void) {

	enable_irqs();
	enable_fiqs();

	uart_print("timer test\n\r");
	init_hw();

//    *armTimerLoad = 0xF0000000; //set load value for 0.5Hz timer
//    *armTimerReload = 0xF0000000;
//    *arm_timer_div = 0x7d; //prescale by 250
//    *armTimerControl = 0x003E00A2;
//    *irqEnableBasic = 1; //clear existing interrupt

	/* Use the ARM timer - BCM 2832 peripherals doc, p.196 */
	/* Enable ARM timer IRQ */
//	*irqEnableBasic = 0x00000001;
//	/* Interrupt every 1024 * 256 (prescaler) timer ticks */
//	*armTimerLoad = 0x40000000; //4000000
//	/* Timer enabled, interrupt enabled, prescale=256, "23 bit" counter
//	 * (did they mean 32 bit?)
//	 */
//	*armTimerControl = 0x000000aa;

//	printH("RPI2 timer CS = %x\n", status());
//	printH("RPI2 timer count = %x\n", count());
//	printH("RPI2 timer cmp 0 = %x\n", cmp(0));
//	printH("RPI2 timer cmp 1 = %x\n", cmp(1));
//	printH("RPI2 timer cmp 2 = %x\n", cmp(2));
//	printH("RPI2 timer cmp 3 = %x\n", cmp(3));
//	printH("\n");
//// trigger on the second next second (at least one second from now)
////	set_cmp(1, count() / 1000000 * 1000000 + 2000000);
//	set_cmp(1, count() + 2000000);
//// clear pending bit and enable irq
//	ctrl_set(1U << 1);
//
//	enable_irq(IRQ_TIMER1);
//	enable_irqs();
////	while (1) {
////// chill out
////		asm volatile ("wfi");
//	}
//	uint32_t interval = 0x00080000;//0xF4240;
//	uint32_t rx = 0;
//
////    interval=0x00080000;
////    rx=GET32(CLO);
////    rx+=interval;
//      rx  = *((volatile uint32_t*) 0x3F003004);
//      printH("rx : %x\n", rx);
//      rx += interval;
//      printH("rx : %x\n", rx);
//      printH("CLO :%x\n", *((volatile uint32_t*) 0x3F003004));
//      printH("CHO :%x\n", *((volatile uint32_t*) 0x3F003008));
////    PUT32(C1,rx);
//    *((volatile uint32_t*) 0x3F003010) = rx;
//
////    PUT32(CS,2);
//    *((volatile uint32_t*) 0x3F003000) = 2;
//
//    //    PUT32(0x2000B210,0x00000002);
//    *((volatile uint32_t*) 0x3F00B210) = 0;
////    PUT32(0x2000B20C,0x80|1);
//    *((volatile uint32_t*) 0x3F00B20C) = (0x80 | 1);
}


inline void nrm_delay(void)
{
    volatile int i = 0;
    for (i = 0; i < 0x0000FFFF; i++)
        ;
}

void nrm_loop(void)
{
    int i = 0;
#if _SMP_
    uint32_t cpu = smp_processor_id();
#endif
#if _CPUISOLATED_
    uint32_t cpu = smp_processor_id();
#endif
    uart_init();
    uart_print(GUEST_LABEL);
    uart_print_hex32(GUEST_NUMBER);

    uart_print("=== Starting commom start up\n\r");

    timer_test();
//    gic_init();
    /* Enables receiving virtual timer interrupt */
//    vtimer_mask(0);
    /* We are ready to accept irqs with GIC. Enable it now */
//    irq_enable();
    /* Test the sample virtual device.
     * - Uncomment the following line of code only if 'vdev_sample' is
     *   registered at the monitor.
     * - Otherwise, the monitor will hang with data abort.
     */
    while(i<100)
    {
    	uart_print("=== Starting commom start up\n\r");
    	i++;
    }
    while(1)
    	;

#ifdef TESTS_ENABLE_VDEV_SAMPLE
    test_vdev_sample();
#endif
    for (i = 0; i < NUM_ITERATIONS; i++) {
        uart_print(GUEST_LABEL);
        uart_print("iteration ");
        uart_print_hex32(i);
        uart_print("\n\r");
        nrm_delay();
#ifdef __MONITOR_CALL_HVC__
        /* Hyp monitor guest run in Non-secure supervisor mode.
         Request test hvc ping and yield one after another
         */
        if (i & 0x1) {
            uart_print(GUEST_LABEL);
            uart_print("hsvc_ping()\n\r");
            hsvc_ping();
            uart_print(GUEST_LABEL);
            uart_print("returned from hsvc_ping()\n\r");
        } else {
            uart_print(GUEST_LABEL);
            uart_print("hsvc_yield()\n\r");
            hsvc_yield();
            uart_print(GUEST_LABEL);
            uart_print("returned from hsvc_yield()\n\r");
        }
#else
        /* Secure monitor guest run in Non-secure supervisor mode
         Request for switch to Secure mode (sec_loop() in the monitor)
         */
        SWITCH_MANUAL();
#endif
        nrm_delay();
    }

    uart_print(GUEST_LABEL);
    uart_print("common nrm_loop done\n\r");
    uart_print("\n[K-HYPERVISOR]TEST#INSTALLATION#BMGUEST-BOOT#PASS\n\r");
    /* start platform start up code */
    main();
}
