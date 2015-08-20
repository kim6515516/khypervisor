#include <gic.h>
#include <armv7_p15.h>
#include <a15_cp15_sysregs.h>
#include <smp.h>
#include <guest.h>
#include <hvmm_trace.h>
#include <gic_regs.h>
#include <hvmm_types.h>
#include <log/print.h>
#include <log/uart_print.h>
#include <k-hypervisor-config.h>

#define CBAR_PERIPHBASE_MSB_MASK    0x000000FF

#define ARM_CPUID_CORTEXA15   0x412fc0f1

#define MIDR_MASK_PPN        (0x0FFF << 4)
#define MIDR_PPN_CORTEXA15    (0xC0F << 4)


#define GIC_INT_PRIORITY_DEFAULT_WORD    ((GIC_INT_PRIORITY_DEFAULT << 24) \
                                         |(GIC_INT_PRIORITY_DEFAULT << 16) \
                                         |(GIC_INT_PRIORITY_DEFAULT << 8) \
                                         |(GIC_INT_PRIORITY_DEFAULT))

#define GIC_SIGNATURE_INITIALIZED   0x5108EAD7
/**
 * @brief Registers for Generic Interrupt Controller(GIC)
 */
struct gic {
    uint32_t baseaddr;          /**< GIC base address */
    volatile uint32_t *ba_gicd; /**< Distributor */
    volatile uint32_t *ba_gicc; /**< CPU interface */
    volatile uint32_t *ba_gich; /**< Virtual interface control (common)*/
    volatile uint32_t *ba_gicv; /**< Virtual interface control (processor-specific) */
    volatile uint32_t *ba_gicvi;/**< Virtual CPU interface */
    uint32_t lines;             /**< The Maximum number of interrupts */
    uint32_t cpus;              /**< The number of implemented CPU interfaces */
    uint32_t initialized;       /**< Check whether initializing GIC. */
};

static struct gic _gic;

static void gic_dump_registers(void)
{
    uint32_t midr;
    HVMM_TRACE_ENTER();
    midr = read_midr();
    uart_print("midr:");
    uart_print_hex32(midr);
    uart_print("\n\r");
    if ((midr & MIDR_MASK_PPN) == MIDR_PPN_CORTEXA15) {
        uint32_t value;
        uart_print("cbar:");
        uart_print_hex32(_gic.baseaddr);
        uart_print("\n\r");
        uart_print("ba_gicd:");
        uart_print_hex32((uint32_t)_gic.ba_gicd);
        uart_print("\n\r");
        uart_print("ba_gicc:");
        uart_print_hex32((uint32_t)_gic.ba_gicc);
        uart_print("\n\r");
        uart_print("ba_gich:");
        uart_print_hex32((uint32_t)_gic.ba_gich);
        uart_print("\n\r");
        uart_print("ba_gicv:");
        uart_print_hex32((uint32_t)_gic.ba_gicv);
        uart_print("\n\r");
        uart_print("ba_gicvi:");
        uart_print_hex32((uint32_t)_gic.ba_gicvi);
        uart_print("\n\r");
        value = _gic.ba_gicd[GICD_CTLR];
        uart_print("GICD_CTLR:");
        uart_print_hex32(value);
        uart_print("\n\r");
        value = _gic.ba_gicd[GICD_TYPER];
        uart_print("GICD_TYPER:");
        uart_print_hex32(value);
        uart_print("\n\r");
        value = _gic.ba_gicd[GICD_IIDR];
        uart_print("GICD_IIDR:");
        uart_print_hex32(value);
        uart_print("\n\r");
    }
    HVMM_TRACE_EXIT();
}

/**
 * @brief Return base address of GIC.
 *
 * When 40 bit address supports, This function wil use.
 */
static uint64_t gic_periphbase_pa(void)
{
    /* CBAR:   4,  c0,   0 */
    /*
     * MRC p15, 4, <Rt>, c15, c0, 0; Read Configuration Base
     * Address Register
     */
    uint64_t periphbase = (uint64_t) read_cbar();
    uint64_t pbmsb = periphbase & ((uint64_t)CBAR_PERIPHBASE_MSB_MASK);
    if (pbmsb) {
        periphbase &= ~((uint64_t)CBAR_PERIPHBASE_MSB_MASK);
        periphbase |= (pbmsb << 32);
    }
    return periphbase;
}
/**
 * @brief   Return address of GIC memory map to _gic.baseaddr.
 * @param   va_base Base address(Physical) of GIC.
 * @return  If target architecture is Cortex-A15 then return success,
 *          otherwise return failed.
 */
static hvmm_status_t gic_init_baseaddr(uint32_t *va_base)
{
    /* MIDR[15:4], CRn:c0, Op1:0, CRm:c0, Op2:0  == 0xC0F (Cortex-A15) */
    /* Cortex-A15 C15 System Control, C15 Registers */
    /* Name: Op1, CRm, Op2 */
    uint32_t midr;
    hvmm_status_t result = HVMM_STATUS_UNKNOWN_ERROR;
    HVMM_TRACE_ENTER();
    midr = read_midr();
    uart_print("midr:");
    uart_print_hex32(midr);
    uart_print("\n\r");
    /*
     * Note:
     * We currently support GICv2 with Cortex-A15 only.
     * Other architectures with GICv2 support will be further
     * listed and added for support later
     */
    if ((midr & MIDR_MASK_PPN) == MIDR_PPN_CORTEXA15) {
        /* fall-back to periphbase addr from cbar */
        if (va_base == 0) {
            va_base = (uint32_t *)(uint32_t)(gic_periphbase_pa() & \
                    0x00000000FFFFFFFFULL);
        }
        uart_print("Cortex-A15 GICv2");
        uart_print("\n\r");
        _gic.baseaddr = (uint32_t) va_base;
        uart_print("cbar:");
        uart_print_hex32(_gic.baseaddr);
        uart_print("\n\r");
        _gic.ba_gicd = (uint32_t *)(_gic.baseaddr + GIC_OFFSET_GICD);
        _gic.ba_gicc = (uint32_t *)(_gic.baseaddr + GIC_OFFSET_GICC);
        _gic.ba_gich = (uint32_t *)(_gic.baseaddr + GIC_OFFSET_GICH);
        _gic.ba_gicv = (uint32_t *)(_gic.baseaddr + GIC_OFFSET_GICV);
        _gic.ba_gicvi = (uint32_t *)(_gic.baseaddr + GIC_OFFSET_GICVI);
        result = HVMM_STATUS_SUCCESS;
    } else if ((midr & MIDR_MASK_PPN) == ( 0xC07 << 4 )) {
        /* fall-back to periphbase addr from cbar */
        if (va_base == 0) {
            va_base = (uint32_t *)(uint32_t)(gic_periphbase_pa() & \
                    0x00000000FFFFFFFFULL);
        }
        uart_print("Cortex-A7 Unsupported GICv2");
        uart_print("\n\r");
        _gic.baseaddr = (uint32_t) va_base;
        uart_print("cbar:");
        uart_print_hex32(_gic.baseaddr);
        uart_print("\n\r");
        _gic.ba_gicd = (uint32_t *)(_gic.baseaddr + GIC_OFFSET_GICD);
        _gic.ba_gicc = (uint32_t *)(_gic.baseaddr + GIC_OFFSET_GICC);
//        _gic.ba_gich = (uint32_t *)(_gic.baseaddr + GIC_OFFSET_GICH);
//        _gic.ba_gicv = (uint32_t *)(_gic.baseaddr + GIC_OFFSET_GICV);
//        _gic.ba_gicvi = (uint32_t *)(_gic.baseaddr + GIC_OFFSET_GICVI);
        result = HVMM_STATUS_SUCCESS;
    } else {
        uart_print("GICv2 Unsupported\n\r");
        uart_print("midr.ppn:");
        uart_print_hex32(midr & MIDR_MASK_PPN);
        uart_print("\n\r");
        result = HVMM_STATUS_UNSUPPORTED_FEATURE;
    }
    HVMM_TRACE_EXIT();
    return result;
}
/**
 * @brief Initializes and enables GIC Distributor
 * <pre>
 * Initialization sequence
 * 1. Set Default SPI's polarity.
 * 2. Set Default priority.
 * 3. Diable all interrupts.
 * 4. Route all IRQs to all target processors.
 * 5. Enable Distributor.
 * </pre>
 * @return Always return success.
 */
static hvmm_status_t gic_init_dist(void)
{
    uint32_t type;
    int i;
    uint32_t cpumask;
    HVMM_TRACE_ENTER();
    /* Disable Distributor */
    _gic.ba_gicd[GICD_CTLR] = 0;
    type = _gic.ba_gicd[GICD_TYPER];
    _gic.lines = 32 * ((type & GICD_TYPE_LINES_MASK) + 1);
    _gic.cpus = 1 + ((type & GICD_TYPE_CPUS_MASK) >> GICD_TYPE_CPUS_SHIFT);
    uart_print("GIC: lines:");
    uart_print_hex32(_gic.lines);
    uart_print(" cpus:");
    uart_print_hex32(_gic.cpus);
    uart_print(" IID:");
    uart_print_hex32(_gic.ba_gicd[GICD_IIDR]);
    uart_print("\n\r");
    /* Interrupt polarity for SPIs (Global Interrupts) active-low */
    for (i = 32; i < _gic.lines; i += 16)
        _gic.ba_gicd[GICD_ICFGR + i / 16] = 0x0;

    /* Default Priority for all Interrupts
     * Private/Banked interrupts will be configured separately
     */
    for (i = 32; i < _gic.lines; i += 4)
        _gic.ba_gicd[GICD_IPRIORITYR + i / 4] = GIC_INT_PRIORITY_DEFAULT_WORD;

    /* Disable all global interrupts.
     * Private/Banked interrupts will be configured separately
     */
    for (i = 32; i < _gic.lines; i += 32)
        _gic.ba_gicd[GICD_ICENABLER + i / 32] = 0xFFFFFFFF;

    /* Route all global IRQs to this CPU */
    cpumask = 1 << smp_processor_id();
    cpumask |= cpumask << 8;
    cpumask |= cpumask << 16;
    for (i = 32; i < _gic.lines; i += 4)
        _gic.ba_gicd[GICD_ITARGETSR + i / 4] = cpumask;

    /* Enable Distributor */
    _gic.ba_gicd[GICD_CTLR] = GICD_CTLR_ENABLE;
    HVMM_TRACE_EXIT();
    return HVMM_STATUS_SUCCESS;
}

/**
 * @brief Initializes GICv2 CPU Interface
 * <pre>
 * Initialization sequence
 * 1. Diable all PPI interrupts, ensure all SGI interrupts are enabled.
 * 2. Set priority on PPI and SGI interrupts.
 * 3. Set priority threshold(Priority Masking),
 *    Finest granularity of priority
 * 4. Enable signaling of interrupts.
 * </pre>
 * @return Always return success.
 */
static hvmm_status_t gic_init_cpui(void)
{
    hvmm_status_t result = HVMM_STATUS_UNKNOWN_ERROR;
    int i;
    /* Disable forwarding PPIs(ID16~31) */
    _gic.ba_gicd[GICD_ICENABLER] = 0xFFFF0000;
    /* Enable forwarding SGIs(ID0~15) */
    _gic.ba_gicd[GICD_ISENABLER] = 0x0000FFFF;
    /* Default priority for SGIs and PPIs */
    for (i = 0; i < 32; i += 4)
        _gic.ba_gicd[GICD_IPRIORITYR + i / 4] = GIC_INT_PRIORITY_DEFAULT_WORD;

    /* No Priority Masking: the lowest value as the threshold : 255 */
    _gic.ba_gicc[GICC_PMR] = 0xFF;
    _gic.ba_gicc[GICC_BPR] = 0x0;
    /* Enable signaling of interrupts and GICC_EOIR only drops priority */
    _gic.ba_gicc[GICC_CTLR] = GICC_CTL_ENABLE | GICC_CTL_EOI;
    result = HVMM_STATUS_SUCCESS;
    return result;
}

/* API functions */
hvmm_status_t gic_enable_irq(uint32_t irq)
{
    _gic.ba_gicd[GICD_ISENABLER + irq / 32] = (1u << (irq % 32));
    return HVMM_STATUS_SUCCESS;
}

hvmm_status_t gic_disable_irq(uint32_t irq)
{
    _gic.ba_gicd[GICD_ICENABLER + irq / 32] = (1u << (irq % 32));
    return HVMM_STATUS_SUCCESS;
}

hvmm_status_t gic_completion_irq(uint32_t irq)
{
    _gic.ba_gicc[GICC_EOIR] = irq;
    return HVMM_STATUS_SUCCESS;
}

hvmm_status_t gic_deactivate_irq(uint32_t irq)
{
    _gic.ba_gicc[GICC_DIR] = irq;
    return HVMM_STATUS_SUCCESS;
}

volatile uint32_t *gic_vgic_baseaddr(void)
{
    if (_gic.initialized != GIC_SIGNATURE_INITIALIZED) {
        HVMM_TRACE_ENTER();
        uart_print("gic: ERROR - not initialized\n\r");
        HVMM_TRACE_EXIT();
    }
    return _gic.ba_gich;
}

hvmm_status_t gic_init(void)
{
    uint32_t cpu = smp_processor_id();
    hvmm_status_t result = HVMM_STATUS_UNKNOWN_ERROR;

    HVMM_TRACE_ENTER();
    /*
     * Determining VA of GIC base adddress has not been defined yet.
     * Let is use the PA for the time being
     */
    if (!cpu) {
        result = gic_init_baseaddr((void *)CFG_GIC_BASE_PA);
        if (result == HVMM_STATUS_SUCCESS)
            gic_dump_registers();
         /*
         * Initialize and Enable GIC Distributor
         */
        if (result == HVMM_STATUS_SUCCESS)
            result = gic_init_dist();
    }
    /*
     * Initialize and Enable GIC CPU Interface for this CPU
     * For test it
     */
    if (cpu)
        result = HVMM_STATUS_SUCCESS;

    if (result == HVMM_STATUS_SUCCESS)
        result = gic_init_cpui();

    if (result == HVMM_STATUS_SUCCESS)
        _gic.initialized = GIC_SIGNATURE_INITIALIZED;

    HVMM_TRACE_EXIT();
    return result;
}

hvmm_status_t gic_configure_irq(uint32_t irq,
                enum gic_int_polarity polarity,  uint8_t cpumask,
                uint8_t priority)
{
    hvmm_status_t result = HVMM_STATUS_UNKNOWN_ERROR;
    HVMM_TRACE_ENTER();
    if (irq < _gic.lines) {
        uint32_t icfg;
        volatile uint8_t *reg8;
        /* disable forwarding */
        result = gic_disable_irq(irq);
        if (result == HVMM_STATUS_SUCCESS) {
            /* polarity: level or edge */
            icfg = _gic.ba_gicd[GICD_ICFGR + irq / 16];
            if (polarity == GIC_INT_POLARITY_LEVEL)
                icfg &= ~(2u << (2 * (irq % 16)));
            else
                icfg |= (2u << (2 * (irq % 16)));

            _gic.ba_gicd[GICD_ICFGR + irq / 16] = icfg;
            /* routing */
            reg8 = (uint8_t *) &(_gic.ba_gicd[GICD_ITARGETSR]);
            reg8[irq] = cpumask;
            /* priority */
            reg8 = (uint8_t *) &(_gic.ba_gicd[GICD_IPRIORITYR]);
            reg8[irq] = priority;
            /* enable forwarding */
            result = gic_enable_irq(irq);
        }
    } else {
        uart_print("invalid irq:");
        uart_print_hex32(irq);
        uart_print("\n\r");
        result = HVMM_STATUS_UNSUPPORTED_FEATURE;
    }
    HVMM_TRACE_EXIT();
    return result;
}


uint32_t gic_get_irq_number(void)
{
    /*
     * 1. ACK - CPU Interface - GICC_IAR read
     * 2. Completion - CPU Interface - GICC_EOIR
     * 2.1 Deactivation - CPU Interface - GICC_DIR
     */
    uint32_t iar;
    uint32_t irq;
    /* ACK */
    iar = _gic.ba_gicc[GICC_IAR];
    irq = iar & GICC_IAR_INTID_MASK;

    return irq;
}

#define BIT(x) (1UL << x)
/* Put the bank and irq (32 bits) into the hwirq */
#define MAKE_HWIRQ(b, n)	(((b) << 5) | (n))
#define HWIRQ_BANK(i)		(i >> 5)
#define HWIRQ_BIT(i)		BIT(i & 0x1f)

#define NR_IRQS_BANK0		8
#define BANK0_HWIRQ_MASK	0xff
/* Shortcuts can't be disabled so any unknown new ones need to be masked */
#define SHORTCUT1_MASK		0x00007c00
#define SHORTCUT2_MASK		0x001f8000
#define SHORTCUT_SHIFT		10
#define BANK1_HWIRQ		BIT(8)
#define BANK2_HWIRQ		BIT(9)
#define BANK0_VALID_MASK	(BANK0_HWIRQ_MASK | BANK1_HWIRQ | BANK2_HWIRQ \
					| SHORTCUT1_MASK | SHORTCUT2_MASK)

#define REG_FIQ_CONTROL		0x0c
#define REG_FIQ_ENABLE		0x80
#define REG_FIQ_DISABLE		0

#define NR_BANKS		3
#define IRQS_PER_BANK		32
#define NUMBER_IRQS		MAKE_HWIRQ(NR_BANKS, 0)
#define FIQ_START		(NR_IRQS_BANK0 + MAKE_HWIRQ(NR_BANKS - 1, 0))
//
//static int reg_pending[]  = { 0x00, 0x04, 0x08 };
//static int reg_enable[]  = { 0x18, 0x10, 0x14 };
//static int reg_disable[]  = { 0x24, 0x1c, 0x20 };
//static int bank_irqs[] *= { 8, 32, 32 };

static const uint32_t shortcuts[] = {
	7, 9, 10, 18, 19,		/* Bank 1 */
	21, 22, 23, 24, 25, 30		/* Bank 2 */
};

/*
 * ffs -- vax ffs instruction
 */
uint32_t ffs(uint32_t mask)
{
	uint32_t bit = 0;

	if (mask == 0)
		return (0);
	for (bit = 1; !(mask & 1); bit++)
		mask >>= 1;
	return (bit);
}



#define readl(b)                __readl(b)
#define readl_relaxed(addr)     readl(addr)



static uint32_t armctrl_handle_bank(int bank)
{
	uint32_t stat, irq = -1;
//	printH("%s irq : %d\n",__func__, irq);
	while(stat = *((volatile uint32_t*)(0x3F00B000 + 0x200 + bank * 0x04))){  // ((stat = readl_relaxed(intc.pending[bank]))) {
		irq = MAKE_HWIRQ(bank, ffs(stat) - 1);
//		handle_IRQ(irq_linear_revmap(intc.domain, irq), regs);
//		printH("%s irq : %d\n",__func__, irq);
		return irq;

	}
	return irq;
}

static uint32_t armctrl_handle_shortcut(int bank, uint32_t stat)
{
	uint32_t irq = MAKE_HWIRQ(bank, shortcuts[ffs(stat >> SHORTCUT_SHIFT) - 1]);
//	handle_IRQ(irq_linear_revmap(intc.domain, irq), regs);
//	printH("%s irq : %d\n",__func__, irq);
	return irq;
}

static uint32_t bcm2835_handle_irq(void)
{
	uint32_t stat;
	uint32_t irq = -1;
//	printH("START: %s irq : %d\n",__func__, irq);
	while ((stat = (*((volatile uint32_t*)(0x3F00B000 + 0x200)) & BANK0_VALID_MASK))) {
		if (stat & BANK0_HWIRQ_MASK) {
			irq = MAKE_HWIRQ(0, ffs(stat & BANK0_HWIRQ_MASK) - 1);
//			printH("%s irq : %d\n",__func__, irq);
//			handle_IRQ(irq_linear_revmap(intc.domain, irq), regs);
			return irq;
		} else if (stat & SHORTCUT1_MASK) {
			irq = armctrl_handle_shortcut(1,  stat & SHORTCUT1_MASK);
			return irq;
		} else if (stat & SHORTCUT2_MASK) {
			irq = armctrl_handle_shortcut(2,  stat & SHORTCUT2_MASK);
			return irq;
		} else if (stat & BANK1_HWIRQ) {
			irq = armctrl_handle_bank(1);
			return irq;
		} else if (stat & BANK2_HWIRQ) {
			irq = armctrl_handle_bank(2);
			return irq;
		} else {
			//
			irq = -1;
		}
	}
	return irq;
}


#define IC_RPI2_BASE_ADDR			0x3F00B000
#define IC_OFFSET_BASEIC_PENDING	0x200
#define IC_OFFSET_PENDING1			0x204
#define IC_OFFSET_PENDING2			0x208
#define IC_OFFSET_FIQ_CONTROL		0x20c
#define IC_OFFSET_ENABLE_IRQS1		0x210
#define IC_OFFSET_ENABLE_IRQS2		0x214
#define IC_OFFSET_ENABLE_BASIC_IRQS	0x218
#define IC_OFFSET_DISABLE_IRQS1		0x21c
#define IC_OFFSET_DISABLE_IRQS2		0x220
#define IC_OFFSET_DISABLE_BASIC_IRQS	0x224

#define clz(a) \
 ({ unsigned long __value, __arg = (a); \
     asm ("clz\t%0, %1": "=r" (__value): "r" (__arg)); \
     __value; })

/**
 *	This is the global IRQ handler on this platform!
 *	It is based on the assembler code found in the Broadcom datasheet.
 *
 **/
int getIrqNumber() {
	register unsigned long ulMaskedStatus = -1;
	register unsigned long ulMaskedStatusTimer = -1;
	register unsigned long irqNumber = -1;

	ulMaskedStatus =  (*((volatile unsigned int*) (IC_RPI2_BASE_ADDR + IC_OFFSET_BASEIC_PENDING)));
	ulMaskedStatusTimer = (*((volatile unsigned int*) (0x40000060)));

	if (!(ulMaskedStatusTimer & 0x100)) {
		ulMaskedStatusTimer&=-ulMaskedStatusTimer;
		/* Some magic to determine number of interrupt to serve */
		irqNumber=96 + 31 - clz(ulMaskedStatusTimer);
		return irqNumber;
	}

	/* Bits 7 through 0 in IRQBasic represent interrupts 64-71 */
	if (ulMaskedStatus & 0xFF) {
		irqNumber=64 + 31;
	}

	/* Bit 8 in IRQBasic indicates interrupts in Pending1 (interrupts 31-0) */
	else if(ulMaskedStatus & 0x100) {
		ulMaskedStatus = (*((volatile unsigned int*) (IC_RPI2_BASE_ADDR + IC_OFFSET_PENDING1)));
		irqNumber = 0 + 31;
	}

	/* Bit 9 in IRQBasic indicates interrupts in Pending2 (interrupts 63-32) */
	else if(ulMaskedStatus & 0x200) {
		ulMaskedStatus = (*((volatile unsigned int*) (IC_RPI2_BASE_ADDR + IC_OFFSET_PENDING2)));
		irqNumber = 32 + 31;
	}

	else {
		// No interrupt avaialbe, so just return.
		return 9999;
	}

	/* Keep only least significant bit, in case multiple interrupts have occured */
	ulMaskedStatus&=-ulMaskedStatus;
	/* Some magic to determine number of interrupt to serve */
	irqNumber=irqNumber-clz(ulMaskedStatus);
	/* Call interrupt handler */
	return irqNumber;
}

uint32_t rpi2_get_irq_number(void)
{

//	uint32_t stat = 0;
//	uint32_t hwirq = -1;
//	stat = *((volatile uint32_t*)(0x3F00B000 + 0x200));
//
////	hwirq = ffs(stat) - 1;
//	if( (stat & (1<<9))) { // pending register2
//		hwirq = *((volatile uint32_t*)(0x3F00B000 + 0x208));
//		printH("pending register2 : %d\n", hwirq);
//	}
//	else if ( (stat & (1<<8))) { // pending register1
//		hwirq = *((volatile uint32_t*)(0x3F00B000 + 0x204));
//		printH("pending register1 : %d\n", hwirq);
//	}

	return getIrqNumber();
}

