#include <k-hypervisor-config.h>
#include <guest.h>
#include <interrupt.h>
#include <timer.h>
#include <vdev.h>
#include <memory.h>
#include <gic_regs.h>
#include <test/tests.h>
#include <smp.h>
#include <asm_io.h>
#include <drivers/mct/mct_priv.h>

#define PLATFORM_BASIC_TESTS 0

#define DECLARE_VIRQMAP(name, id, _pirq, _virq) \
    do {                                        \
        name[id].map[_pirq].virq = _virq;       \
        name[id].map[_virq].pirq = _pirq;       \
    } while (0)

static struct guest_virqmap _guest_virqmap[NUM_GUESTS_STATIC];

static struct memmap_desc guest_md_empty[] = {
    {       0, 0, 0, 0,  0},
};

/*  label, ipa, pa, size, attr */
static struct memmap_desc guest0_device_md[] = {
	{ "uart", 0x12C10000, 0x12C10000, 0x1000, MEMATTR_DM },
    { 0, 0, 0, 0, 0 }
};


static struct memmap_desc guest1_device_md[] = {
    { "uart", 0x12C10000, 0x12C10000, 0x1000, MEMATTR_DM },
    { "uart2", 0x12C20000, 0x12C20000, 0x1000, MEMATTR_DM },
    { "gpio1", 0x11400000, 0x11400000, SZ_4K, MEMATTR_DM },
    { "gpio2", 0x13400000, 0x13400000, SZ_4K, MEMATTR_DM },
    { "gpio3", 0x10D10000, 0x10D10000, SZ_256, MEMATTR_DM },
    { "gpio4", 0x03860000, 0x03860000, SZ_256, MEMATTR_DM },
    { "pwm_timer", 0x3FD10000, 0x12DD0000, 0x1000, MEMATTR_DM },
    { "gicc", CFG_GIC_BASE_PA | GIC_OFFSET_GICC,
        CFG_GIC_BASE_PA | GIC_OFFSET_GICVI, 0x2000, MEMATTR_DM },
    { 0, 0, 0, 0, 0 }
};

#if _SMP_
static struct memmap_desc guest2_device_md[] = {
    { "uart", 0x12C10000, 0x12C00000, 0x1000, MEMATTR_DM },
    { "pwm_timer", 0x3FD10000, 0x12DD0000, 0x1000, MEMATTR_DM },
    { "gpio1", 0x11400000, 0x11400000, SZ_4K, MEMATTR_DM },
    { "gpio2", 0x13400000, 0x13400000, SZ_4K, MEMATTR_DM },
    { "gpio3", 0x10D10000, 0x10D10000, SZ_256, MEMATTR_DM },
    { "gpio4", 0x03860000, 0x03860000, SZ_256, MEMATTR_DM },
    { "gicc", CFG_GIC_BASE_PA | GIC_OFFSET_GICC,
        CFG_GIC_BASE_PA | GIC_OFFSET_GICVI, 0x2000, MEMATTR_DM },
    { 0, 0, 0, 0, 0 }
};

static struct memmap_desc guest3_device_md[] = {
    { "uart", 0x12C10000, 0x12C00000, 0x1000, MEMATTR_DM },
    { "pwm_timer", 0x3FD10000, 0x12DD0000, 0x1000, MEMATTR_DM },
    { "gpio1", 0x11400000, 0x11400000, SZ_4K, MEMATTR_DM },
    { "gpio2", 0x13400000, 0x13400000, SZ_4K, MEMATTR_DM },
    { "gpio3", 0x10D10000, 0x10D10000, SZ_256, MEMATTR_DM },
    { "gpio4", 0x03860000, 0x03860000, SZ_256, MEMATTR_DM },
    { "gicc", CFG_GIC_BASE_PA | GIC_OFFSET_GICC,
        CFG_GIC_BASE_PA | GIC_OFFSET_GICVI, 0x2000, MEMATTR_DM },
    { 0, 0, 0, 0, 0 }
};
#endif

static struct memmap_desc guest0_memory_md[] = {
    {"start", 0x00000000, 0x10000000, 0x30000000,
     MEMATTR_NORMAL_OWB | MEMATTR_NORMAL_IWB
    },
    { "uart", 0x3F200000, 0x3F200000, SZ_1M, MEMATTR_DM },
    { "afdf", 0x3F400000, 0x3F400000, SZ_1M, MEMATTR_DM },
    {0, 0, 0, 0,  0},
};

static struct memmap_desc guest1_memory_md[] = {
    /* 256MB */
	    {"start", 0x00000000, 0x20000000, 0x30000000,
	     MEMATTR_NORMAL_OWB | MEMATTR_NORMAL_IWB
	    },
	    { "uart", 0x3F200000, 0x3F200000, SZ_1M, MEMATTR_DM },
	    { "afdf", 0x3F400000, 0x3F400000, SZ_1M, MEMATTR_DM },
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

/* Memory Map for Guest 0 */
static struct memmap_desc *guest0_mdlist[] = {
		guest0_memory_md,   /* 0x0000_0000 */
		guest_md_empty,   /* 0x4000_0000 */
    guest_md_empty,
    guest_md_empty,     /* 0xC000_0000 PA:0x40000000*/
    0
};

/* Memory Map for Guest 1 */
static struct memmap_desc *guest1_mdlist[] = {
		guest1_memory_md,
		guest_md_empty,
    guest_md_empty,
    guest_md_empty,
    0
};

#if _SMP_
/* Memory Map for Guest 2 */
static struct memmap_desc *guest2_mdlist[] = {
    guest2_device_md,
    guest2_memory_md,
    guest_md_empty,
    guest_md_empty,
    0
};

/* Memory Map for Guest 3 */
static struct memmap_desc *guest3_mdlist[] = {
    guest3_device_md,
    guest3_memory_md,
    guest_md_empty,
    guest_md_empty,
    0
};
#endif

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

    for (i = 32; i < 64; i++)
        DECLARE_VIRQMAP(_guest_virqmap, 0, i, i);

    for (i = 68; i < 73; i++)
        DECLARE_VIRQMAP(_guest_virqmap, 0, i, i);

    DECLARE_VIRQMAP(_guest_virqmap, 0, 27, 27);
    DECLARE_VIRQMAP(_guest_virqmap, 0, 64, 64);
    DECLARE_VIRQMAP(_guest_virqmap, 0, 66, 66);
    DECLARE_VIRQMAP(_guest_virqmap, 0, 67, 67);
    DECLARE_VIRQMAP(_guest_virqmap, 0, 84, 84);
    DECLARE_VIRQMAP(_guest_virqmap, 0, 85, 85);
    DECLARE_VIRQMAP(_guest_virqmap, 0, 88, 88);
    DECLARE_VIRQMAP(_guest_virqmap, 0, 90, 90);
    DECLARE_VIRQMAP(_guest_virqmap, 0, 96, 96);
	DECLARE_VIRQMAP(_guest_virqmap, 0, 103, 103);
    DECLARE_VIRQMAP(_guest_virqmap, 0, 104, 104);
    DECLARE_VIRQMAP(_guest_virqmap, 0, 107, 107);
    DECLARE_VIRQMAP(_guest_virqmap, 0, 108, 108);
    DECLARE_VIRQMAP(_guest_virqmap, 0, 109, 109);
    DECLARE_VIRQMAP(_guest_virqmap, 0, 126, 126);
    DECLARE_VIRQMAP(_guest_virqmap, 0, 147, 147);
    DECLARE_VIRQMAP(_guest_virqmap, 0, 156, 156);
    DECLARE_VIRQMAP(_guest_virqmap, 0, 347, 347);

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
//    printH("Setup Memory guest 1: %x\n", _guest0_bin_start);
//    printH("Setup Memory guest 2: %x\n", _guest1_bin_start);

#if _SMP_
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
    mct_init();
}

#include <io-exynos.h>
extern void init_secondary();

int main_cpu_init()
{

	 uint32_t cpu = smp_processor_id();
	 if(cpu){
		 printH("[%s : %d]SMP on....\n", __func__, __LINE__);
		 while(1)
			 ;
	 }
    init_print();

#ifdef _SMP_
    /*
     * In Exnosy5250 platform like a Arndale, secondary cpus have been WFE
     * state by u-boot spl,so hyperviosr should wake up secondary cpu using
     * the sev instruction. This code will be different on each platform.
     */
    dsb_sev();
#endif
    printH("[%s : %d]Starting...Main CPU\n", __func__, __LINE__);
    setup_memory();
    /* Initialize Memory Management */
    if (memory_init(guest0_mdlist, guest1_mdlist))
        printh("[start_guest] virtual memory initialization failed...\n");

    /* Initialize PIRQ to VIRQ mapping */
    setup_interrupt();
    /* Initialize Interrupt Management */
//    if (interrupt_init(_guest_virqmap))
//        printh("[start_guest] interrupt initialization failed...\n");

#ifdef _SMP_
    printH("wake up...other CPUs\n");
    writel((unsigned int)init_secondary, 0x02020000);
#endif

    /* Initialize Timer */
    setup_timer();
//    if (timer_init(_timer_irq))
//        printh("[start_guest] timer initialization failed...\n");

    /* Initialize Guests */
    if (guest_init())
        printh("[start_guest] guest initialization failed...\n");

    /* Initialize Virtual Devices */
    if (vdev_init())
        printh("[start_guest] virtual device initialization failed...\n");

    /* Begin running test code for newly implemented features */
    if (basic_tests_run(PLATFORM_BASIC_TESTS))
        printh("[start_guest] basic testing failed...\n");

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

    printH("[%s : %d] Starting...Secondary CPU\n", __func__, __LINE__);

    /* Initialize Memory Management */
    if (memory_init(guest2_mdlist, guest3_mdlist))
        printh("[start_guest] virtual memory initialization failed...\n");

    printH("[%s : %d] Interrupt Init... for CPU\n", __func__, __LINE__);

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


int main(void)
{
#ifdef _SMP_
    uint32_t cpu = smp_processor_id();

    if (!cpu)
        main_cpu_init();
    else
        secondary_cpu_init(cpu);
#else
    main_cpu_init();
#endif
    return 0;
}
