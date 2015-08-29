#include <stdint.h>
#include <asm-arm_inline.h>
#include <log/uart_print.h>
#include <gic.h>
#include <test/tests.h>
#include <drivers/pwm_timer.h>


#define GPFSEL1 0x3F200004
#define GPSET0 0x3F20001C
#define GPCLR0 0x3F200028


/* #define TESTS_ENABLE_PWM_TIMER */

int main()
{
    uart_print(GUEST_LABEL);
    uart_print("=== Starting platform main, LED ON/OFF ===\n\r");
//#ifdef TESTS_ENABLE_PWM_TIMER
//    hvmm_tests_pwm_timer();
//#endif


    unsigned int ra;
	ra = GET32(GPFSEL1);
	ra &= ~(7 << 18);
	ra |= 1 << 18;
	PUT32(GPFSEL1, ra);
	while (1) {
		PUT32(GPSET0, 1 << 16);
		 uart_print("=BMGUEST: LED ON ===\n\r");
		for (ra = 0; ra < 0x100000; ra++)
			dummy(ra);
		PUT32(GPCLR0, 1 << 16);
		uart_print("=BMGUESTL: LED OFF ===\n\r");
		for (ra = 0; ra < 0x100000; ra++)
			dummy(ra);
	}

    while (1)
        ;
    return 0;
}
