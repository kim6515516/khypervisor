#include <k-hypervisor-config.h>
#include <guest.h>
#include <interrupt.h>
#include <timer.h>
#include <vdev.h>
#include <memory.h>
#include <gic_regs.h>
#include <test/tests.h>
#include <smp.h>

#define DEBUG
#include "hvmm_trace.h"
#include <log/uart_print.h>


#define PLATFORM_BASIC_TESTS 4

#define DECLARE_VIRQMAP(name, id, _pirq, _virq) \
    do {                                        \
        name[id].map[_pirq].virq = _virq;       \
        name[id].map[_virq].pirq = _pirq;       \
    } while (0)


static struct guest_virqmap _guest_virqmap[NUM_GUESTS_STATIC];

/**
 * \defgroup Guest_memory_map_descriptor
 *
 * Descriptor setting order
 * - label
 * - Intermediate Physical Address (IPA)
 * - Physical Address (PA)
 * - Size of memory region
 * - Memory Attribute
 * @{
 */
static struct memmap_desc guest_md_empty[] = {
    {       0, 0, 0, 0,  0},
};
/*  label, ipa, pa, size, attr */
static struct memmap_desc guest0_device_md[] = {
    { "sysreg", 0x1C010000, 0x1C010000, SZ_4K, MEMATTR_DM },
    { "sysctl", 0x1C020000, 0x1C020000, SZ_4K, MEMATTR_DM },
    { "aaci", 0x1C040000, 0x1C040000, SZ_4K, MEMATTR_DM },
    { "mmci", 0x1C050000, 0x1C050000, SZ_4K, MEMATTR_DM },
    { "kmi", 0x1C060000, 0x1C060000,  SZ_64K, MEMATTR_DM },
    { "kmi2", 0x1C070000, 0x1C070000, SZ_64K, MEMATTR_DM },
    { "v2m_serial0", 0x1C090000, 0x1C0A0000, SZ_4K, MEMATTR_DM },
    { "v2m_serial1", 0x1C0A0000, 0x1C090000, SZ_4K, MEMATTR_DM },
    { "v2m_serial2", 0x1C0B0000, 0x1C0B0000, SZ_4K, MEMATTR_DM },
    { "v2m_serial3", 0x1C0C0000, 0x1C0C0000, SZ_4K, MEMATTR_DM },
    { "wdt", 0x1C0F0000, 0x1C0F0000, SZ_4K, MEMATTR_DM },
    { "v2m_timer01(sp804)", 0x1C110000, 0x1C110000, SZ_4K,
            MEMATTR_DM },
    { "v2m_timer23", 0x1C120000, 0x1C120000, SZ_4K, MEMATTR_DM },
    { "rtc", 0x1C170000, 0x1C170000, SZ_4K, MEMATTR_DM },
    { "clcd", 0x1C1F0000, 0x1C1F0000, SZ_4K, MEMATTR_DM },
//    { "gicc", CFG_GIC_BASE_PA | GIC_OFFSET_GICC,
//            CFG_GIC_BASE_PA | GIC_OFFSET_GICVI, SZ_8K,
//            MEMATTR_DM },
    { "SMSC91c111i", 0x1A000000, 0x1A000000, SZ_16M, MEMATTR_DM },
    { "simplebus2", 0x18000000, 0x18000000, SZ_64M, MEMATTR_DM },
    { "simplebus2", 0x00160000, 0x00160000, SZ_64M, MEMATTR_DM },
    { "q", 0x1C014000, 0x1C014000, SZ_64K, MEMATTR_DM },
//    { "b", 0xFFFF0000, 0xFFFF0000, SZ_64M, MEMATTR_DM },
    { 0, 0, 0, 0, 0 }
};

static struct memmap_desc guest1_device_md[] = {
	    { "sysreg", 0x1C010000, 0x1C010000, SZ_4K, MEMATTR_DM },
	    { "sysctl", 0x1C020000, 0x1C020000, SZ_4K, MEMATTR_DM },
	    { "aaci", 0x1C040000, 0x1C040000, SZ_4K, MEMATTR_DM },
	    { "mmci", 0x1C050000, 0x1C050000, SZ_4K, MEMATTR_DM },
	    { "kmi", 0x1C060000, 0x1C060000,  SZ_64K, MEMATTR_DM },
	    { "kmi2", 0x1C070000, 0x1C070000, SZ_64K, MEMATTR_DM },
	    { "v2m_serial0", 0x1C090000, 0x1C0B0000, SZ_4K, MEMATTR_DM },
	    { "v2m_serial1", 0x1C0A0000, 0x1C090000, SZ_4K, MEMATTR_DM },
	    { "v2m_serial2", 0x1C0B0000, 0x1C0B0000, SZ_4K, MEMATTR_DM },
	    { "v2m_serial3", 0x1C0C0000, 0x1C0C0000, SZ_4K, MEMATTR_DM },
	    { "wdt", 0x1C0F0000, 0x1C0F0000, SZ_4K, MEMATTR_DM },
	    { "v2m_timer01(sp804)", 0x1C110000, 0x1C110000, SZ_4K,
	            MEMATTR_DM },
	    { "v2m_timer23", 0x1C120000, 0x1C120000, SZ_4K, MEMATTR_DM },
	    { "rtc", 0x1C170000, 0x1C170000, SZ_4K, MEMATTR_DM },
	    { "clcd", 0x1C1F0000, 0x1C1F0000, SZ_4K, MEMATTR_DM },
	//    { "gicc", CFG_GIC_BASE_PA | GIC_OFFSET_GICC,
	//            CFG_GIC_BASE_PA | GIC_OFFSET_GICVI, SZ_8K,
	//            MEMATTR_DM },
	    { "SMSC91c111i", 0x1A000000, 0x1A000000, SZ_16M, MEMATTR_DM },
	    { "simplebus2", 0x18000000, 0x18000000, SZ_64M, MEMATTR_DM },
	    { "simplebus2", 0x00160000, 0x00160000, SZ_64M, MEMATTR_DM },
	    { "q", 0x1C014000, 0x1C014000, SZ_64K, MEMATTR_DM },
	//    { "b", 0xFFFF0000, 0xFFFF0000, SZ_64M, MEMATTR_DM },

//    { "uart", 0x1C090000, 0x1C0B0000, SZ_4K, MEMATTR_DM },
//    { "sp804", 0x1C110000, 0x1C120000, SZ_4K, MEMATTR_DM },
//    { "gicc", 0x2C000000 | GIC_OFFSET_GICC,
//       CFG_GIC_BASE_PA | GIC_OFFSET_GICVI, SZ_8K, MEMATTR_DM },
    {0, 0, 0, 0, 0}
};

#if _SMP_
static struct memmap_desc guest2_device_md[] = {
    { "uart", 0x1C090000, 0x1C0B0000, SZ_4K, MEMATTR_DM },
    { "sp804", 0x1C110000, 0x1C120000, SZ_4K, MEMATTR_DM },
    { "gicc", 0x2C000000 | GIC_OFFSET_GICC,
       CFG_GIC_BASE_PA | GIC_OFFSET_GICVI, SZ_8K, MEMATTR_DM },
    {0, 0, 0, 0, 0}
};

static struct memmap_desc guest3_device_md[] = {
    { "uart", 0x1C090000, 0x1C0B0000, SZ_4K, MEMATTR_DM },
    { "sp804", 0x1C110000, 0x1C120000, SZ_4K, MEMATTR_DM },
    { "gicc", 0x2C000000 | GIC_OFFSET_GICC,
       CFG_GIC_BASE_PA | GIC_OFFSET_GICVI, SZ_8K, MEMATTR_DM },
    {0, 0, 0, 0, 0}
};
#endif
#if _CPUISOLATED_
static struct memmap_desc guest2_device_md[] = {
	    { "sysreg", 0x1C010000, 0x1C010000, SZ_4K, MEMATTR_DM },
	    { "sysctl", 0x1C020000, 0x1C020000, SZ_4K, MEMATTR_DM },
	    { "aaci", 0x1C040000, 0x1C040000, SZ_4K, MEMATTR_DM },
	    { "mmci", 0x1C050000, 0x1C050000, SZ_4K, MEMATTR_DM },
	    { "kmi", 0x1C060000, 0x1C060000,  SZ_64K, MEMATTR_DM },
	    { "kmi2", 0x1C070000, 0x1C070000, SZ_64K, MEMATTR_DM },
	    { "v2m_serial0", 0x1C090000, 0x1C0B0000, SZ_4K, MEMATTR_DM },
	    { "v2m_serial1", 0x1C0A0000, 0x1C0B0000, SZ_4K, MEMATTR_DM },
	    { "v2m_serial2", 0x1C0B0000, 0x1C0B0000, SZ_4K, MEMATTR_DM },
	    { "v2m_serial3", 0x1C0C0000, 0x1C0C0000, SZ_4K, MEMATTR_DM },
	    { "wdt", 0x1C0F0000, 0x1C0F0000, SZ_4K, MEMATTR_DM },
	    { "v2m_timer01(sp804)", 0x1C110000, 0x1C110000, SZ_4K,
	            MEMATTR_DM },
	    { "v2m_timer23", 0x1C120000, 0x1C120000, SZ_4K, MEMATTR_DM },
	    { "rtc", 0x1C170000, 0x1C170000, SZ_4K, MEMATTR_DM },
	    { "clcd", 0x1C1F0000, 0x1C1F0000, SZ_4K, MEMATTR_DM },
	    { "gicc", CFG_GIC_BASE_PA | GIC_OFFSET_GICC,
	            CFG_GIC_BASE_PA | GIC_OFFSET_GICVI, SZ_8K,
	            MEMATTR_DM },
	    { "SMSC91c111i", 0x1A000000, 0x1A000000, SZ_16M, MEMATTR_DM },
	    { "simplebus2", 0x18000000, 0x18000000, SZ_64M, MEMATTR_DM },

	    { 0, 0, 0, 0, 0 }
};

static struct memmap_desc guest3_device_md[] = {
	    { "uart", 0x1C090000, 0x1C0B0000, SZ_4K, MEMATTR_DM },
	    { "sp804", 0x1C110000, 0x1C120000, SZ_4K, MEMATTR_DM },
	    { "gicc", 0x2C000000 | GIC_OFFSET_GICC,
	       CFG_GIC_BASE_PA | GIC_OFFSET_GICVI, SZ_8K, MEMATTR_DM },
	    {0, 0, 0, 0, 0}
};
#endif
/**
 * @brief Memory map for guest 0.
 */
static struct memmap_desc guest0_memory_md[] = {
    {"start", 0x00000000, 0, 0x40000000,
     MEMATTR_NORMAL_OWB | MEMATTR_NORMAL_IWB
    },
    {0, 0, 0, 0,  0},
};

/**
 * @brief Memory map for guest 1.
 */
static struct memmap_desc guest1_memory_md[] = {
    /* 256MB */
    {"start", 0x00000000, 0, 0x10000000,
     MEMATTR_NORMAL_OWB | MEMATTR_NORMAL_IWB
    },
    {0, 0, 0, 0,  0},
};

#if _SMP_
/**
 * @brief Memory map for guest 2.
 */
static struct memmap_desc guest2_memory_md[] = {
    /* 256MB */
    {"start", 0x00000000, 0, 0x10000000,
     MEMATTR_NORMAL_OWB | MEMATTR_NORMAL_IWB
    },
    {0, 0, 0, 0,  0},
};

/**
 * @brief Memory map for guest 3.
 */
static struct memmap_desc guest3_memory_md[] = {
    /* 256MB */
    {"start", 0x00000000, 0, 0x10000000,
     MEMATTR_NORMAL_OWB | MEMATTR_NORMAL_IWB
    },
    {0, 0, 0, 0,  0},
};
#endif
#if _CPUISOLATED_
/**
 * @brief Memory map for guest 2.
 */
static struct memmap_desc guest2_memory_md[] = {
    /* 256MB */
    {"start", 0x00000000, 0, 0x10000000,
     MEMATTR_NORMAL_OWB | MEMATTR_NORMAL_IWB
    },
    {0, 0, 0, 0,  0},
};

/**
 * @brief Memory map for guest 3.
 */
static struct memmap_desc guest3_memory_md[] = {
    /* 256MB */
    {"start", 0x00000000, 0, 0x10000000,
     MEMATTR_NORMAL_OWB | MEMATTR_NORMAL_IWB
    },
    {0, 0, 0, 0,  0},
};
#endif
/* Memory Map for Guest 0 */
static struct memmap_desc *guest0_mdlist[] = {
    guest0_device_md,   /* 0x0000_0000 */
    guest_md_empty,     /* 0x4000_0000 */
    guest0_memory_md,
    guest_md_empty,     /* 0xC000_0000 PA:0x40000000*/
    0
};

/* Memory Map for Guest 1 */
static struct memmap_desc *guest1_mdlist[] = {
    guest1_device_md,
    guest_md_empty,
    guest1_memory_md,
    guest_md_empty,
    0
};

#if _SMP_
/* Memory Map for Guest 2 */
static struct memmap_desc *guest2_mdlist[] = {
    guest2_device_md,
    guest_md_empty,
    guest2_memory_md,
    guest_md_empty,
    0
};

/* Memory Map for Guest 3 */
static struct memmap_desc *guest3_mdlist[] = {
    guest3_device_md,
    guest_md_empty,
    guest3_memory_md,
    guest_md_empty,
    0
};
#endif
#if _CPUISOLATED_
/* Memory Map for Guest 2 */
static struct memmap_desc *guest2_mdlist[] = {
    guest2_device_md,
    guest_md_empty,
    guest2_memory_md,
    guest_md_empty,
    0
};

/* Memory Map for Guest 3 */
static struct memmap_desc *guest3_mdlist[] = {
    guest3_device_md,
    guest_md_empty,
    guest3_memory_md,
    guest_md_empty,
    0
};
#endif
/** @}*/

static uint32_t _timer_irq;

/*
 * Creates a mapping table between PIRQ and VIRQ.vmid/pirq/coreid.
 * Mapping of between pirq and virq is hard-coded.
 */
void setup_interrupt()
{
    int i, j;
    struct virqmap_entry *map;

    for (i = 0; i < NUM_GUESTS_STATIC; i++) {
        map = _guest_virqmap[i].map;
        for (j = 0; j < MAX_IRQS; j++) {
            map[j].enabled = GUEST_IRQ_DISABLE;
            map[j].virq = VIRQ_INVALID;
            map[j].pirq = PIRQ_INVALID;
        }
    }

    /*
     *  vimm-0, pirq-69, virq-69 = pwm timer driver
     *  vimm-0, pirq-32, virq-32 = WDT: shared driver
     *  vimm-0, pirq-34, virq-34 = SP804: shared driver
     *  vimm-0, pirq-35, virq-35 = SP804: shared driver
     *  vimm-0, pirq-36, virq-36 = RTC: shared driver
     *  vimm-0, pirq-38, virq-37 = UART: dedicated driver IRQ 37 for guest 0
     *  vimm-1, pirq-39, virq-37 = UART: dedicated driver IRQ 37 for guest 1
     *  vimm-2, pirq,40, virq-37 = UART: dedicated driver IRQ 37 for guest 2
     *  vimm-3, pirq,48, virq-37 = UART: dedicated driver IRQ 38 for guest 3 -ch
     *  vimm-0, pirq-43, virq-43 = ACCI: shared driver
     *  vimm-0, pirq-44, virq-44 = KMI: shared driver
     *  vimm-0, pirq-45, virq-45 = KMI: shared driver
     *  vimm-0, pirq-47, virq-47 = SMSC 91C111, Ethernet - etc0
     *  vimm-0, pirq-41, virq-41 = MCI - pl180
     *  vimm-0, pirq-42, virq-42 = MCI - pl180
     */
//    for (i=0 ; i< MAX_IRQS; i++) {
//    	if(i != 26)
//    		DECLARE_VIRQMAP(_guest_virqmap, 0, i, i);
//    }
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 1, 1);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 16, 16);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 17, 17);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 18, 18);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 19, 19);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 31, 31);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 32, 32);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 33, 33);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 30, 30);
    DECLARE_VIRQMAP(_guest_virqmap, 0, 34, 34);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 35, 35);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 36, 36);
//
//
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 37, 37);
    DECLARE_VIRQMAP(_guest_virqmap, 0, 38, 37);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 39, 39);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 40, 40);
//
////    DECLARE_VIRQMAP(_guest_virqmap, 0, 37, 38);
////    DECLARE_VIRQMAP(_guest_virqmap, 0, 38, 37);
////    DECLARE_VIRQMAP(_guest_virqmap, 1, 39, 37);
////    DECLARE_VIRQMAP(_guest_virqmap, 2, 40, 37);
////    DECLARE_VIRQMAP(_guest_virqmap, 3, 48, 37);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 41, 41);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 42, 42);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 43, 43);
    DECLARE_VIRQMAP(_guest_virqmap, 0, 44, 44);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 45, 45);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 46, 46);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 47, 47);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 48, 48);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 49, 49);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 50, 50);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 51, 51);
//    DECLARE_VIRQMAP(_guest_virqmap, 0, 69, 69);
//
//
//
//    DECLARE_VIRQMAP(_guest_virqmap, 2, 1, 1);
//    DECLARE_VIRQMAP(_guest_virqmap, 2, 16, 16);
//    DECLARE_VIRQMAP(_guest_virqmap, 2, 17, 17);
//    DECLARE_VIRQMAP(_guest_virqmap, 2, 18, 18);
//    DECLARE_VIRQMAP(_guest_virqmap, 2, 19, 19);
//    DECLARE_VIRQMAP(_guest_virqmap, 2, 31, 31);
//    DECLARE_VIRQMAP(_guest_virqmap, 2, 32, 32);
//    DECLARE_VIRQMAP(_guest_virqmap, 2, 33, 33);
    DECLARE_VIRQMAP(_guest_virqmap, 1, 34, 34);
//    DECLARE_VIRQMAP(_guest_virqmap, 2, 35, 35);
//    DECLARE_VIRQMAP(_guest_virqmap, 2, 36, 36);
//    DECLARE_VIRQMAP(_guest_virqmap, 2, 37, 38);
//    DECLARE_VIRQMAP(_guest_virqmap, 2, 38, 37);
    DECLARE_VIRQMAP(_guest_virqmap, 1, 39, 37);
//    DECLARE_VIRQMAP(_guest_virqmap, 2, 40, 37);
//    DECLARE_VIRQMAP(_guest_virqmap, 2, 48, 37);
//    DECLARE_VIRQMAP(_guest_virqmap, 2, 41, 41);
//    DECLARE_VIRQMAP(_guest_virqmap, 2, 42, 42);
//    DECLARE_VIRQMAP(_guest_virqmap, 2, 43, 43);
    DECLARE_VIRQMAP(_guest_virqmap, 1, 44, 44);
//    DECLARE_VIRQMAP(_guest_virqmap, 2, 45, 45);
//    DECLARE_VIRQMAP(_guest_virqmap, 2, 46, 46);
//    DECLARE_VIRQMAP(_guest_virqmap, 2, 47, 47);
//    DECLARE_VIRQMAP(_guest_virqmap, 2, 69, 69);

}

void setup_memory()
{
    /*
     * VA: 0x00000000 ~ 0x3FFFFFFF,   1GB
     * PA: 0xA0000000 ~ 0xDFFFFFFF    guest_bin_start
     * PA: 0xB0000000 ~ 0xEFFFFFFF    guest2_bin_start
     */
    guest0_memory_md[0].pa = (uint64_t)((uint32_t) &_guest0_bin_start);
    guest1_memory_md[0].pa = (uint64_t)((uint32_t) &_guest1_bin_start);
#if _SMP_
    guest2_memory_md[0].pa = (uint64_t)((uint32_t) &_guest2_bin_start);
    guest3_memory_md[0].pa = (uint64_t)((uint32_t) &_guest3_bin_start);
#endif
#if _CPUISOLATED_
    guest2_memory_md[0].pa = (uint64_t)((uint32_t) &_guest2_bin_start);
    guest3_memory_md[0].pa = (uint64_t)((uint32_t) &_guest3_bin_start);
#endif
}

/** @brief Registers generic timer irqs such as hypervisor timer event
 *  (GENERIC_TIMER_HYP), non-secure physical timer event(GENERIC_TIMER_NSP)
 *  and virtual timer event(GENERIC_TIMER_NSP).
 *  Each interrup source is identified by a unique ID.
 *  cf. "Cortex™-A15 Technical Reference Manual" 8.2.3 Interrupt sources
 *
 *  DEVICE : IRQ number
 *  GENERIC_TIMER_HYP : 26
 *  GENERIC_TIMER_NSP : 30
 *  GENERIC_TIMER_VIR : 27
 *
 *  @note "Cortex™-A15 Technical Reference Manual", 8.2.3 Interrupt sources
 */
void setup_timer()
{
    _timer_irq = 26; /* GENERIC_TIMER_HYP */
}

uint8_t secondary_smp_pen;

int main_cpu_init()
{
    init_print();
    printH("[%s : %d] Starting...Main CPU\n", __func__, __LINE__);

    /* Initialize Memory Management */
    setup_memory();

    if (memory_init(guest0_mdlist, guest1_mdlist))
        printh("[start_guest] virtual memory initialization failed...\n");
    /* Initialize PIRQ to VIRQ mapping */
    setup_interrupt();
    /* Initialize Interrupt Management */
    if (interrupt_init(_guest_virqmap))
        printh("[start_guest] interrupt initialization failed...\n");

#ifdef _SMP_
    printH("wake up...other CPUs\n");
    secondary_smp_pen = 1;
#endif
#ifdef _CPUISOLATED_
    printH("wake up...other CPUs\n");
    secondary_smp_pen = 1;
#endif
    /* Initialize Timer */
    setup_timer();
    if (timer_init(_timer_irq))
        printh("[start_guest] timer initialization failed...\n");

    /* Initialize Guests */
    if (guest_init())
        printh("[start_guest] guest initialization failed...\n");

    /* Initialize Virtual Devices */
    if (vdev_init())
        printh("[start_guest] virtual device initialization failed...\n");

    /* Begin running test code for newly implemented features */
//    if (basic_tests_run(TESTS_ENABLE_GIC_TIMER))
//        printh("[start_guest] basic testing failed...\n");

    /* Print Banner */
    printH("%s", BANNER_STRING);

    /* Switch to the first guest */
    guest_sched_start();

    /* The code flow must not reach here */
    printh("[hyp_main] ERROR: CODE MUST NOT REACH HERE\n");
    hyp_abort_infinite();
}

#ifdef _SMP_

void secondary_cpu_init(uint32_t cpu)
{
    if (cpu >= CFG_NUMBER_OF_CPUS)
        hyp_abort_infinite();

    init_print();
    printH("[%s : %d] Starting...CPU : #%d\n", __func__, __LINE__, cpu);

    /* Initialize Memory Management */
    if (memory_init(guest2_mdlist, guest3_mdlist))
        printh("[start_guest] virtual memory initialization failed...\n");

    /* Initialize Interrupt Management */
    if (interrupt_init(_guest_virqmap))
        printh("[start_guest] interrupt initialization failed...\n");

    /* Initialize Timer */
    if (timer_init(_timer_irq))
        printh("[start_guest] timer initialization failed...\n");

    /* Initialize Guests */
    if (guest_init())
        printh("[start_guest] guest initialization failed...\n");

    /* Initialize Virtual Devices */
    if (vdev_init())
        printh("[start_guest] virtual device initialization failed...\n");

    /* Switch to the first guest */
    guest_sched_start();

    /* The code flow must not reach here */
    printh("[hyp_main] ERROR: CODE MUST NOT REACH HERE\n");
    hyp_abort_infinite();
}

#endif
#ifdef _CPUISOLATED_

void secondary_cpu_init(uint32_t cpu)
{
    if (cpu >= 1)
        hyp_abort_infinite();

    init_print();
    printH("[%s : %d] Starting...CPU : #%d\n", __func__, __LINE__, cpu);

    /* Initialize Memory Management */
    if (memory_init(guest2_mdlist, guest3_mdlist))
        printh("[start_guest] virtual memory initialization failed...\n");

    /* Initialize Interrupt Management */
    if (interrupt_init(_guest_virqmap))
        printh("[start_guest] interrupt initialization failed...\n");

    /* Initialize Timer */
    if (timer_init(_timer_irq))
        printh("[start_guest] timer initialization failed...\n");

    /* Initialize Guests */
    if (guest_init())
        printh("[start_guest] guest initialization failed...\n");

//    /* Initialize Virtual Devices */
//    if (vdev_init())
//        printh("[start_guest] virtual device initialization failed...\n");
    /* Begin running test code for newly implemented features */
    if (basic_tests_run(TESTS_ENABLE_GIC_TIMER))
        printh("[start_guest] basic testing failed...\n");
    /* Switch to the first guest */
    guest_sched_start();

    /* The code flow must not reach here */
    printh("[hyp_main] ERROR: CODE MUST NOT REACH HERE\n");
    hyp_abort_infinite();
}

#endif
int main(void)
{
#ifdef _SMP_
    uint32_t cpu = smp_processor_id();

    if (cpu)
        secondary_cpu_init(cpu);
    else
#endif
#ifdef _CPUISOLATED_
    uint32_t cpu = smp_processor_id();

    if (cpu)
        secondary_cpu_init(cpu);
    else
#endif
        main_cpu_init();

    return 0;
}
