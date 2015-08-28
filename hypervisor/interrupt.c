#define DEBUG
#include <hvmm_types.h>
#include <guest.h>
#include <hvmm_trace.h>
#include <log/uart_print.h>
#include <interrupt.h>
#include <smp.h>
#include <guest.h>


#define VIRQ_MIN_VALID_PIRQ 16
#define VIRQ_NUM_MAX_PIRQS  MAX_IRQS

#define VALID_PIRQ(pirq) \
    (pirq >= VIRQ_MIN_VALID_PIRQ && pirq < VIRQ_NUM_MAX_PIRQS)


static struct interrupt_ops *_guest_ops;
static struct interrupt_ops *_host_ops;

static struct guest_virqmap *_guest_virqmap;

/**< IRQ handler */
static interrupt_handler_t _host_ppi_handlers[NUM_CPUS][MAX_PPI_IRQS];
static interrupt_handler_t _host_spi_handlers[MAX_IRQS];

const int32_t interrupt_check_guest_irq(uint32_t pirq)
{
    int i;
    struct virqmap_entry *map;

    for (i = 0; i < NUM_GUESTS_STATIC; i++) {
        map = _guest_virqmap[i].map;
        if (map[pirq].virq != VIRQ_INVALID)
            return GUEST_IRQ;
    }

    return HOST_IRQ;
}

const uint32_t interrupt_pirq_to_virq(vmid_t vmid, uint32_t pirq)
{
    struct virqmap_entry *map = _guest_virqmap[vmid].map;

    return map[pirq].virq;
}

const uint32_t interrupt_virq_to_pirq(vmid_t vmid, uint32_t virq)
{
    struct virqmap_entry *map = _guest_virqmap[vmid].map;

    return map[virq].pirq;
}

const uint32_t interrupt_pirq_to_enabled_virq(vmid_t vmid, uint32_t pirq)
{
    uint32_t virq = VIRQ_INVALID;
    struct virqmap_entry *map = _guest_virqmap[vmid].map;

    if (map[pirq].enabled)
        virq = map[pirq].virq;

    return virq;
}

hvmm_status_t interrupt_guest_inject(vmid_t vmid, uint32_t virq, uint32_t pirq,
                uint8_t hw)
{
    hvmm_status_t ret = HVMM_STATUS_UNKNOWN_ERROR;

    /* guest_interrupt_inject() */
    if (_guest_ops->inject)
        ret = _guest_ops->inject(vmid, virq, pirq, hw);

    return ret;
}

hvmm_status_t interrupt_request(uint32_t irq, interrupt_handler_t handler)
{
    uint32_t cpu = smp_processor_id();

    if (irq < MAX_PPI_IRQS)
        _host_ppi_handlers[cpu][irq] = handler;

    else
        _host_spi_handlers[irq] = handler;

    return HVMM_STATUS_SUCCESS;
}

hvmm_status_t interrupt_host_enable(uint32_t irq)
{
    hvmm_status_t ret = HVMM_STATUS_UNKNOWN_ERROR;

    /* host_interrupt_enable() */
    if (_host_ops->enable)
        ret = _host_ops->enable(irq);

    return ret;
}

hvmm_status_t interrupt_host_disable(uint32_t irq)
{
    hvmm_status_t ret = HVMM_STATUS_UNKNOWN_ERROR;

    /* host_interrupt_disable() */
    if (_host_ops->disable)
        ret = _host_ops->disable(irq);

    return ret;
}

hvmm_status_t interrupt_host_configure(uint32_t irq)
{
    hvmm_status_t ret = HVMM_STATUS_UNKNOWN_ERROR;

    /* host_interrupt_configure() */
    if (_host_ops->configure)
        ret = _host_ops->configure(irq);

    return ret;
}

hvmm_status_t interrupt_guest_enable(vmid_t vmid, uint32_t irq)
{
    hvmm_status_t ret = HVMM_STATUS_UNKNOWN_ERROR;
    struct virqmap_entry *map = _guest_virqmap[vmid].map;

    map[irq].enabled = GUEST_IRQ_ENABLE;

    return ret;
}

hvmm_status_t interrupt_guest_disable(vmid_t vmid, uint32_t irq)
{
    hvmm_status_t ret = HVMM_STATUS_UNKNOWN_ERROR;
    struct virqmap_entry *map = _guest_virqmap[vmid].map;

    map[irq].enabled = GUEST_IRQ_DISABLE;

    return ret;
}

static void interrupt_inject_enabled_guest(int num_of_guests, uint32_t irq)
{
    int i;
    uint32_t virq;

    for (i = 0; i < num_of_guests; i++) {
        virq = interrupt_pirq_to_enabled_virq(i, irq);
        if (virq == VIRQ_INVALID)
            continue;
        interrupt_guest_inject(i, virq, irq, INJECT_HW);
    }
}

static void context_save_banked(struct regs_banked *regs_banked)
{
    /* USR banked register */
    asm volatile(" mrs     %0, sp_usr\n\t"
                 : "=r"(regs_banked->sp_usr) : : "memory", "cc");
    /* SVC banked register */
    asm volatile(" mrs     %0, spsr_svc\n\t"
                 : "=r"(regs_banked->spsr_svc) : : "memory", "cc");
    asm volatile(" mrs     %0, sp_svc\n\t"
                 : "=r"(regs_banked->sp_svc) : : "memory", "cc");
    asm volatile(" mrs     %0, lr_svc\n\t"
                 : "=r"(regs_banked->lr_svc) : : "memory", "cc");
    /* ABT banked register */
    asm volatile(" mrs     %0, spsr_abt\n\t"
                 : "=r"(regs_banked->spsr_abt) : : "memory", "cc");
    asm volatile(" mrs     %0, sp_abt\n\t"
                 : "=r"(regs_banked->sp_abt) : : "memory", "cc");
    asm volatile(" mrs     %0, lr_abt\n\t"
                 : "=r"(regs_banked->lr_abt) : : "memory", "cc");
    /* UND banked register */
    asm volatile(" mrs     %0, spsr_und\n\t"
                 : "=r"(regs_banked->spsr_und) : : "memory", "cc");
    asm volatile(" mrs     %0, sp_und\n\t"
                 : "=r"(regs_banked->sp_und) : : "memory", "cc");
    asm volatile(" mrs     %0, lr_und\n\t"
                 : "=r"(regs_banked->lr_und) : : "memory", "cc");
    /* IRQ banked register */
    asm volatile(" mrs     %0, spsr_irq\n\t"
                 : "=r"(regs_banked->spsr_irq) : : "memory", "cc");
    asm volatile(" mrs     %0, sp_irq\n\t"
                 : "=r"(regs_banked->sp_irq) : : "memory", "cc");
    asm volatile(" mrs     %0, lr_irq\n\t"
                 : "=r"(regs_banked->lr_irq) : : "memory", "cc");
    /* FIQ banked register  R8_fiq ~ R12_fiq, LR and SPSR */
    asm volatile(" mrs     %0, spsr_fiq\n\t"
                 : "=r"(regs_banked->spsr_fiq) : : "memory", "cc");
    asm volatile(" mrs     %0, lr_fiq\n\t"
                 : "=r"(regs_banked->lr_fiq) : : "memory", "cc");
    asm volatile(" mrs     %0, r8_fiq\n\t"
                 : "=r"(regs_banked->r8_fiq) : : "memory", "cc");
    asm volatile(" mrs     %0, r9_fiq\n\t"
                 : "=r"(regs_banked->r9_fiq) : : "memory", "cc");
    asm volatile(" mrs     %0, r10_fiq\n\t"
                 : "=r"(regs_banked->r10_fiq) : : "memory", "cc");
    asm volatile(" mrs     %0, r11_fiq\n\t"
                 : "=r"(regs_banked->r11_fiq) : : "memory", "cc");
    asm volatile(" mrs     %0, r12_fiq\n\t"
                 : "=r"(regs_banked->r12_fiq) : : "memory", "cc");

    asm volatile(" mrs     %0, spsr_hyp\n\t"
                 : "=r"(regs_banked->spsr_hyp) : : "memory", "cc");
    asm volatile(" mrs     %0, elr_hyp\n\t"
                 : "=r"(regs_banked->elr_hyp) : : "memory", "cc");
}
static void context_save_cops(struct regs_cop *regs_cop)
{
    regs_cop->vbar = read_vbar();
    regs_cop->ttbr0 = read_ttbr0();
    regs_cop->ttbr1 = read_ttbr1();
    regs_cop->ttbcr = read_ttbcr();
    regs_cop->sctlr = read_sctlr();
}
static void context_copy_banked(struct regs_banked *banked_dst, struct
        regs_banked *banked_src)
{
    banked_dst->sp_usr = banked_src->sp_usr;
    banked_dst->spsr_svc = banked_src->spsr_svc;
    banked_dst->sp_svc = banked_src->sp_svc;
    banked_dst->lr_svc = banked_src->lr_svc;
    banked_dst->spsr_abt = banked_src->spsr_abt;
    banked_dst->sp_abt = banked_src->sp_abt;
    banked_dst->lr_abt = banked_src->lr_abt;
    banked_dst->spsr_und = banked_src->spsr_und;
    banked_dst->sp_und = banked_src->sp_und;
    banked_dst->lr_und = banked_src->sp_und;
    banked_dst->spsr_irq = banked_src->spsr_irq;
    banked_dst->sp_irq = banked_src->sp_irq;
    banked_dst->lr_irq = banked_src->lr_irq;
    banked_dst->spsr_fiq = banked_src->spsr_fiq;
    banked_dst->lr_fiq = banked_src->lr_fiq;
    banked_dst->r8_fiq = banked_src->r8_fiq;
    banked_dst->r9_fiq = banked_src->r9_fiq;
    banked_dst->r10_fiq = banked_src->r10_fiq;
    banked_dst->r11_fiq = banked_src->r11_fiq;
    banked_dst->r12_fiq = banked_src->r12_fiq;
}
static void context_copy_regs(struct arch_regs *regs_dst,
                struct arch_regs *regs_src)
{
    int i;
    regs_dst->cpsr = regs_src->cpsr;
    regs_dst->pc = regs_src->pc;
    regs_dst->lr = regs_src->lr;
    for (i = 0; i < ARCH_REGS_NUM_GPR; i++)
        regs_dst->gpr[i] = regs_src->gpr[i];
}
static struct guest_struct *target;
void changeGuestMode(int irq, void *current_regs)
{
	struct arch_regs *regs = (struct arch_regs *)current_regs;
    int i = 0;
    int ci = -1;
	hvmm_status_t ret = HVMM_STATUS_SUCCESS;
//    guest_hw_dump_extern(0x4, current_regs);
//    printH("enter changeGuest mode IRQ : %d\n", irq);
    uint32_t lr, sp;
    target = get_guest_pointer(guest_current_vmid());
    if (target)
    	;
    else
    	printH("target is null : %d\n", irq);

    ci  = vdev_find_tag(0, 66);
//    if (vdev_execute(0, ci, 2, 0) != 0x000003FF) {
//    	printH("iar is not 0x000003FF : \n");
//    	return;
//    }

    context_save_banked(&target->context.regs_banked);
    context_save_cops(&target->context.regs_cop);
//    target->vmid = 0;
    context_copy_regs(target, regs);


//    printH("start!!\n");
//    printH("show sp's %x \n", target->context.regs_banked.sp_usr);
//    printH("show sp's %x \n", target->context.regs_banked.sp_abt);
//    printH("show sp's %x \n", target->context.regs_banked.sp_und);
//    printH("show sp's %x \n", target->context.regs_banked.sp_irq);
//
//    asm volatile(" mrs     %0, sp_svc\n\t" : "=r"(sp) : : "memory", "cc");
//    printH("show sp's %x \n", sp);
//    asm volatile(" mrs     %0, sp_usr\n\t" : "=r"(sp) : : "memory", "cc");
//    printH("show sp's %x \n", sp);
//    printH("end!!\n");
//    guest_hw_dump_extern(0x4, target);
//    printH("sctlr : %x\n",target->context.regs_cop.sctlr);
//    printH("ttbcr : %x\n",target->context.regs_cop.ttbcr);
//    printH("ttbr0 : %x\n",target->context.regs_cop.ttbr0);
//    printH("ttbr1 : %x\n",target->context.regs_cop.ttbr1);
//    printH("vbar : %x\n",target->context.regs_cop.vbar);

//    if(target->context.regs_cop.ttbr1 == 0) {
//    	 printH("ttbr is 0, guest n: %d\n", guest_current_vmid());
//    	 vdev_execute(0, ci, 1, irq); //irq inject
//    	return;
//    }
//    int check = target->regs.cpsr & (0x1 << 7);
//    if ( check )
//    {
//    	printH("Guest IRQ is disalbe\n");
//    	vdev_execute(0, ci, 0, irq); //irq pendding
//    	return;
//    }
//    else
//    	printH("Guest IRQ not disalbe\n");

//    printH("target vbar : %x\n", target->context.regs_cop.vbar);
    target->context.regs_banked.spsr_irq = target->regs.cpsr;
    target->regs.cpsr = target->regs.cpsr & ~(0x1 << 5);
    target->regs.cpsr = target->regs.cpsr | (0x1 << 7);
//    target->regs.cpsr = target->regs.cpsr & ~(0x1F);
//    printH("changeGuest before_cpsr : %x\n", target->regs.cpsr);
    target->regs.cpsr = target->regs.cpsr & ~(0x1F);

    target->regs.cpsr = target->regs.cpsr | 0x12;
//    printH("changeGuest after_cpsr : %x\n", target->regs.cpsr);
//    printH("changeGuest before_pc : %x\n", target->regs.pc);
//    printH("changeGuest elr_hyp : %x\n", target->context.regs_banked.elr_hyp);
//    if(target->regs.cpsr & 0x10)
//    	target->context.regs_banked.lr_irq = target->regs.pc;
//    else
    	target->context.regs_banked.lr_irq = target->regs.pc + 4;

//    if (guest_current_vmid() == 0)  // linux
//    target->regs.pc = 0xffff0018;
    target->regs.pc = 0xffff0018;
//    else // bm guest
//    	target->regs.pc = 0xffff0038;
//    target->regs.gpr[13] = 0x80B55FA8;
//    target->regs.lr = target->regs.pc - 4;
//    target->regs.pc = 0x8000DA00;
//    printH("changeGuest after_pc : %x\n", target->regs.pc);
//    printH("changeGuest cpsr : %x\n", target->regs.cpsr);
//    _guest_ops->end(irq);
//    guest_switchto(0, 0);


//    *addr = 34;
//    printH("changeGuest iar : %x\n", *addr);


//    printH("find dev : %d \n", ci);
//    vdev_execute(0, ci, 0, 0x200); //irq inject
//    vdev_execute(0, ci, 2, irq); //irq inject
//    if(irq == 99)
//    	vdev_execute(0, ci, 4, irq); //irq inject
//    else
//    	vdev_execute(0, ci, 3, irq); //irq inject
//    printH("changeGuest. switch forced\n");
    perform_switch_forced2(target, 0);
    timerReset2();
    regs->cpsr = target->regs.cpsr;
    regs->lr = target->regs.lr;
    regs->pc = target->regs.pc;
//    printH("changeGuest. end\n");
//    _mon_switch_to_guest_context(&target->regs);
}
int c  =0 ;


#define CS      0x3F003000
#define CLO     0x3F003004
#define C0      0x3F00300C
#define C1      0x3F003010
#define C2      0x3F003014
#define C3      0x3F003018

#define INTERVAL 0x10000000


#define ENABLE_TIMER_IRQ() PUT32(CS,0x8)
#define DISABLE_TIMER_IRQ() PUT32(CS,~2);


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
//
#define clz(a) \
 ({ unsigned long __value, __arg = (a); \
     asm ("clz\t%0, %1": "=r" (__value): "r" (__arg)); \
     __value; })
//
///**
// *	This is the global IRQ handler on this platform!
// *	It is based on the assembler code found in the Broadcom datasheet.
// *
// **/
//int getIrqNumber() {
//	register unsigned long ulMaskedStatus = -1;
//	register unsigned long irqNumber = -1;
//
//	ulMaskedStatus =  (*((volatile unsigned int*) (IC_RPI2_BASE_ADDR + IC_OFFSET_BASEIC_PENDING)));
//
//	/* Bits 7 through 0 in IRQBasic represent interrupts 64-71 */
//	if (ulMaskedStatus & 0xFF) {
//		irqNumber=64 + 31;
//	}
//
//	/* Bit 8 in IRQBasic indicates interrupts in Pending1 (interrupts 31-0) */
//	else if(ulMaskedStatus & 0x100) {
//		ulMaskedStatus = (*((volatile unsigned int*) (IC_RPI2_BASE_ADDR + IC_OFFSET_PENDING1)));
//		irqNumber = 0 + 31;
//	}
//
//	/* Bit 9 in IRQBasic indicates interrupts in Pending2 (interrupts 63-32) */
//	else if(ulMaskedStatus & 0x200) {
//		ulMaskedStatus = (*((volatile unsigned int*) (IC_RPI2_BASE_ADDR + IC_OFFSET_PENDING2)));
//		irqNumber = 32 + 31;
//	}
//
//	else {
//		// No interrupt avaialbe, so just return.
//		return -1;
//	}
//
//	/* Keep only least significant bit, in case multiple interrupts have occured */
//	ulMaskedStatus&=-ulMaskedStatus;
//	/* Some magic to determine number of interrupt to serve */
//	irqNumber=irqNumber-clz(ulMaskedStatus);
//	/* Call interrupt handler */
//	return irqNumber;
//}
//

void printIC(void) {
	unsigned long ulMaskedStatus;
	unsigned long irqNumber;

	printH("IC_OFFSET_LOCAL_IRQ_PENDING0 : %x\n", (*((volatile unsigned int*) (0x40000060)))  );
	printH("IC_OFFSET_LOCAL_IRQ_PENDING0 f: %x\n", (*((volatile unsigned int*) (0xf4000060)))  );

	printH("IC_OFFSET_BASEIC_PENDING : %x\n", (*((volatile unsigned int*) (IC_RPI2_BASE_ADDR + IC_OFFSET_BASEIC_PENDING))));
	printH("IC_OFFSET_PENDING1 : %x\n", (*((volatile unsigned int*) (IC_RPI2_BASE_ADDR + IC_OFFSET_PENDING1))));
	printH("IC_OFFSET_PENDING2 : %x\n", (*((volatile unsigned int*) (IC_RPI2_BASE_ADDR + IC_OFFSET_PENDING2))));

	printH("IC_OFFSET_FIQ_CONTROL : %x\n", (*((volatile unsigned int*) (IC_RPI2_BASE_ADDR + IC_OFFSET_FIQ_CONTROL))));
	printH("IC_OFFSET_ENABLE_IRQS1 : %x\n", (*((volatile unsigned int*) (IC_RPI2_BASE_ADDR + IC_OFFSET_ENABLE_IRQS1))));
	printH("IC_OFFSET_ENABLE_IRQS2 : %x\n", (*((volatile unsigned int*) (IC_RPI2_BASE_ADDR + IC_OFFSET_ENABLE_IRQS2))));

	printH("IC_OFFSET_ENABLE_BASIC_IRQS : %x\n", (*((volatile unsigned int*) (IC_RPI2_BASE_ADDR + IC_OFFSET_ENABLE_BASIC_IRQS))));
	printH("IC_OFFSET_DISABLE_IRQS1 : %x\n", (*((volatile unsigned int*) (IC_RPI2_BASE_ADDR + IC_OFFSET_DISABLE_IRQS1))));
	printH("IC_OFFSET_DISABLE_IRQS2 : %x\n", (*((volatile unsigned int*) (IC_RPI2_BASE_ADDR + IC_OFFSET_DISABLE_IRQS2))));
	printH("IC_OFFSET_DISABLE_BASIC_IRQS : %x\n", (*((volatile unsigned int*) (IC_RPI2_BASE_ADDR + IC_OFFSET_DISABLE_BASIC_IRQS))));

//	ulMaskedStatus = (*((volatile unsigned int*) (IC_RPI2_BASE_ADDR + IC_OFFSET_PENDING1)));
//	irqNumber = 31;
//	ulMaskedStatus&=-ulMaskedStatus;
//	irqNumber=irqNumber-clz(ulMaskedStatus);
	printH("IRQ = %x\n", getIrqNumber());


}
enum generic_timer_type {
    GENERIC_TIMER_HYP,      /* IRQ 26 */
    GENERIC_TIMER_VIR,      /* IRQ 27 */
    GENERIC_TIMER_NSP,      /* IRQ 30 */
    GENERIC_TIMER_NUM_TYPES
};

enum {
    GENERIC_TIMER_REG_FREQ,
    GENERIC_TIMER_REG_HCTL,
    GENERIC_TIMER_REG_KCTL,
    GENERIC_TIMER_REG_HYP_CTRL,
    GENERIC_TIMER_REG_HYP_TVAL,
    GENERIC_TIMER_REG_HYP_CVAL,
    GENERIC_TIMER_REG_PHYS_CTRL,
    GENERIC_TIMER_REG_PHYS_TVAL,
    GENERIC_TIMER_REG_PHYS_CVAL,
    GENERIC_TIMER_REG_VIRT_CTRL,
    GENERIC_TIMER_REG_VIRT_TVAL,
    GENERIC_TIMER_REG_VIRT_CVAL,
    GENERIC_TIMER_REG_VIRT_OFF,
};
inline void generic_timer_reg_write(int reg, uint32_t val)
{
    switch (reg) {
    case GENERIC_TIMER_REG_FREQ:
        write_cntfrq(val);
        break;
    case GENERIC_TIMER_REG_HCTL:
        write_cnthctl(val);
        break;
    case GENERIC_TIMER_REG_KCTL:
        write_cntkctl(val);
        break;
    case GENERIC_TIMER_REG_HYP_CTRL:
        write_cnthp_ctl(val);
        break;
    case GENERIC_TIMER_REG_HYP_TVAL:
        write_cnthp_tval(val);
        break;
    case GENERIC_TIMER_REG_PHYS_CTRL:
        write_cntp_ctl(val);
        break;
    case GENERIC_TIMER_REG_PHYS_TVAL:
        write_cntp_tval(val);
        break;
    case GENERIC_TIMER_REG_VIRT_CTRL:
        write_cntv_ctl(val);
        break;
    case GENERIC_TIMER_REG_VIRT_TVAL:
        write_cntv_tval(val);
        break;
    default:
        uart_print("Trying to write invalid generic-timer register\n\r");
        break;
    }
    isb();
}
static inline uint32_t generic_timer_reg_read(int reg)
{
    uint32_t val;
    switch (reg) {
    case GENERIC_TIMER_REG_FREQ:
        val = read_cntfrq();
        break;
    case GENERIC_TIMER_REG_HCTL:
        val = read_cnthctl();
        break;
    case GENERIC_TIMER_REG_KCTL:
        val = read_cntkctl();
        break;
    case GENERIC_TIMER_REG_HYP_CTRL:
        val = read_cnthp_ctl();
        break;
    case GENERIC_TIMER_REG_HYP_TVAL:
        val = read_cnthp_tval();
        break;
    case GENERIC_TIMER_REG_PHYS_CTRL:
        val = read_cntp_ctl();
        break;
    case GENERIC_TIMER_REG_PHYS_TVAL:
        val = read_cntp_tval();
        break;
    case GENERIC_TIMER_REG_VIRT_CTRL:
        val = read_cntv_ctl();
        break;
    case GENERIC_TIMER_REG_VIRT_TVAL:
        val = read_cntv_tval();
        break;
    default:
        uart_print("Trying to read invalid generic-timer register\n\r");
        break;
    }
    return val;
}
void timerReset(void){
    generic_timer_reg_write(GENERIC_TIMER_REG_HYP_CTRL, 0x7);
    generic_timer_reg_write(GENERIC_TIMER_REG_HYP_TVAL, 0x150000);
    generic_timer_reg_write(GENERIC_TIMER_REG_HYP_CTRL, 0x5);
}
void timerReset2(void){
    generic_timer_reg_write(GENERIC_TIMER_REG_VIRT_CTRL, 0x7);
//    generic_timer_reg_write(GENERIC_TIMER_REG_VIRT_TVAL, 0x085000);
    generic_timer_reg_write(GENERIC_TIMER_REG_VIRT_TVAL, 0x120000);

    generic_timer_reg_write(GENERIC_TIMER_REG_VIRT_CTRL, 0x5);
    isb();
}

#define read_cntvoff()          ({ uint32_t v1, v2; asm volatile(\
                                " mrrc     p15, 4, %0, %1, c14\n\t" \
                                : "=r" (v1), "=r" (v2) : : "memory", "cc"); \
                                (((uint64_t)v2 << 32) + (uint64_t)v1); })

#define write_cntvoff(val)    asm volatile(\
                              " mcrr     p15, 4, %0, %1, c14\n\t" \
                              : : "r" ((val) & 0xFFFFFFFF), "r" ((val) >> 32) \
                              : "memory", "cc")

int isUart = 0;
void interrupt_service_routine(int irq, void *current_regs, void *pdata)
{
    struct arch_regs *regs = (struct arch_regs *)current_regs;
    uint32_t vmid =  guest_current_vmid();
    uint32_t cpu = smp_processor_id();
    uint32_t rx = 0;
    int ci, cr ;
    ci  = vdev_find_tag(0, 66);
//	printH("irq:%d, c: %d, pc:%x, cpsr:%x, vid=%d\n", irq, c, regs->pc, regs->cpsr, vmid);
//    (regs->cpsr & 0x1F) != 0x13)
//    if(((regs->cpsr & 0x1F) != 0x10) && ((regs->cpsr & 0x1F) != 0x1f) || (vmid != 0) ) {  // not user mode, s
		if( (vmid == 1) && (irq != 98) && (irq != 99) ){ // pending..
//	    	printH("IRQ pending!!%d, vmid:%d\n" ,irq, vmid);
	//    	timerReset();
//			printH("irq:%d, c: %d, pc:%x, cpsr:%x, vid=%d\n", irq, c, regs->pc, regs->cpsr, vmid);
			timerReset2();

			if(irq == 65)
				*((int*)0x3F00B224) = 0x2;
			if(irq == 66 )
				 *((int*)0x3F00B224) = 0x4;
			if(irq == 9999)
				*((int*)0x3F00B220) = 0x02000000;


			 vdev_execute(0, ci, 5, irq );
			 vdev_execute(0, ci, 5, irq );
			 return;
		} else if ( (vmid==0) && ((regs->cpsr & 0x1F) != 0x13) && ((regs->cpsr & 0x1F) != 0x10) && ((regs->cpsr & 0x1F) != 0x1f)) {
			timerReset2();
//			printH("c: %d, pc:%x, cpsr:%x, vid=%d\n", c, regs->pc, regs->cpsr, vmid);

			if(irq == 65)
				*((int*)0x3F00B224) = 0x2;
			if(irq == 66 )
				 *((int*)0x3F00B224) = 0x4;
			if(irq == 9999)
				*((int*)0x3F00B220) = 0x02000000;
			 vdev_execute(0, ci, 5, irq );
			 vdev_execute(0, ci, 5, irq );
			 return;
		}
//    }

    cr = vdev_execute(0, ci, 7, -5);
    if(cr > 0 && (vmid==0) && (irq == 99) ){   // pendding check
//    	printH("IRQ execute pending:%d\n", cr);
//    	printH("e irq:%d, c: %d, pc:%x, cpsr:%x, vid=%d\n", irq, c, regs->pc, regs->cpsr, vmid);
    	vdev_execute(0, ci, 6, irq );
//    	timerReset();

    	ci  = vdev_find_tag(0, 77);
        vdev_execute(0, ci, 1, 0x110 );
        timerReset2();
		changeGuestMode(irq, regs);
		return;
    }

    if( irq == 98) // hyper
    {

    	timerReset();
    	if( _guest_module.ops->init)
    		guest_switchto(sched_policy_determ_next(), 0);
//    	printH("irq: 98\n");
    	return;
    }


    if (irq == 99 ) // vtimer
    {
    	int ci;
        c++;
//        if( guest_current_vmid()!=0)
//        {
//        	c++;
//        	timerReset2();
//        	return;
//        }

        if(c%2==0){
        	// switching
    		timerReset2();
    		if( _guest_module.ops->init)
    		    		guest_switchto(sched_policy_determ_next(), 0);

    		if (c > 100 && vmid ==0){
    		       	 ci  = vdev_find_tag(0, 77);
    		//       	 printH("aaaaaaaaaa\n");
    		//        	 vdev_execute(0, ci, 1, (*((volatile unsigned int*) (0x40000060))) | 0x100 );
    		       	 vdev_execute(0, ci, 1, 0x08);
    		       	 timerReset2();
    		   		changeGuestMode(99, regs);
    		}
//        } else if (c > 100 && vmid ==0){
//       	 ci  = vdev_find_tag(0, 77);
////       	 printH("aaaaaaaaaa\n");
////        	 vdev_execute(0, ci, 1, (*((volatile unsigned int*) (0x40000060))) | 0x100 );
//       	 vdev_execute(0, ci, 1, 0x08);
//       	 timerReset2();
//   		changeGuestMode(99, regs);
        }

        return;



    } else if(irq == 65 && vmid == 0){

			int ci  = vdev_find_tag(0, 77);

			 vdev_execute(0, ci, 1, 0x110 );

			 ci  = vdev_find_tag(0, 66);
			 vdev_execute(0, ci, 3, -5 );
			 *((int*)0x3F00B224) = 0x2;

			changeGuestMode(irq, regs);

    } else if(irq == 66 && vmid == 0){
//    	printH("!!!!!!!!!!!!!!!!!!!!!!!!!!! IRQ : %d\n", irq);
		int ci  = vdev_find_tag(0, 77);

		 vdev_execute(0, ci, 1, 0x100 );

		 ci  = vdev_find_tag(0, 66);
		 vdev_execute(0, ci, 3, -5 );
		 *((int*)0x3F00B224) = 0x4;

//		 timerReset2();
		changeGuestMode(irq, regs);
    } else if (irq == 9999 && vmid ==0){
    	if(isUart == 0)
    		isUart =  1;
//    	printH("!!!!!!!!!!!!!!!!!!!!!!!!!!! IRQ : %d\n", irq);

		int ci  = vdev_find_tag(0, 77);
	//        	 vdev_execute(0, ci, 1, (*((volatile unsigned int*) (0x40000060))) | 0x100 );
		 vdev_execute(0, ci, 1, 0x110 );
		 ci  = vdev_find_tag(0, 66);
//		 vdev_execute(0, ci, 3, -5 );
		 vdev_execute(0, ci, 4, -5 );
		 *((int*)0x3F00B220) = 0x02000000;

//		 timerReset2();
		changeGuestMode(irq, regs);
//

    } else {
    	printH("!!!!!!!!!!!!!!!!!!!!!!!!!!! IRQ : %d\n", irq);
    	while(1);
    }

    return;
    if(irq == 0xffffffff)
    {

        rx=GET32(CLO);
        rx += INTERVAL;
        PUT32(C3,rx);
        c++;
//        printIC();
        if(c > 3000) {
        	if(guest_current_vmid()==0)
        	{
//        		if(c == 3001 || c == 3002 )
//        		c= 0;
//        		 printH("isr c !!!!!!!!!!!!!!!!: %d\n", c);
        		changeGuestMode(99, regs);
        	}
        }

    	if( _guest_module.ops->init)
    		guest_switchto(sched_policy_determ_next(), 0);


    } else if(irq == 0xf){
//    	changeGuestMode(irq, regs);
//		 printH(" c 222222: %d %d\n", c, irq);
//    	if((guest_current_vmid()==0) && (irq == 83)) {
//    		printH("irq 83!!!\n");
//    		changeGuestMode(irq, regs);
//
//    	}
//    	c++;
//    	if(c > 3000){
//    		printH("30000\n");
//			if(irq != 0xf)
//				changeGuestMode(irq, regs);
//    	} else
//    		 printH(" c !!!!: %d %d\n", c, irq);
//    		printH("isr IRQ ###: %x\n", irq);
//    	printIC();

    } else {

    }


    return;


//    changeGuestMode(irq, regs);

    if ( irq == 39 || irq == 38 )
    {
    	printH("irq 39\n");
    	while(1) ;
    }
    if(cpu) {
    	printH("second cpu isr.\n");
    	return ;
    }

    if (irq < MAX_IRQS) {
        if (interrupt_check_guest_irq(irq) == GUEST_IRQ) {
//        	printH("check guest irq. end: %d\n", irq);
#ifdef _SMP_
            /*
             * workaround for arndale port
             * We will identify the interrupt number 0, which is
             * an unknown number.
             */
            if (cpu) {
                if (irq == 0) {
                    /* ignore injecting the guest */
                    _guest_ops->end(irq);
                    return;
                }
            }
#endif
            /* IRQ INJECTION */
            /* priority drop only for hanlding irq in guest */
            /* guest_interrupt_end() */
//            if(guest_current_vmid()!=0)
//            	_guest_ops->end(irq);
//            else
//            	changeGuestMode(irq, current_regs) ;
//            interrupt_inject_enabled_guest(NUM_GUESTS_STATIC, irq);
        } else {
            /* host irq */
        	printH("check host irq. end: %d\n", irq);
            if (irq < MAX_PPI_IRQS) {
                if (_host_ppi_handlers[cpu][irq])
                    _host_ppi_handlers[cpu][irq](irq, regs, 0);
            } else {
                if (_host_spi_handlers[irq])
                    _host_spi_handlers[irq](irq, regs, 0);
            }
            /* host_interrupt_end() */

            _host_ops->end(irq);
//            printH("!!!!!!!!!!!!!!!!!!!!!!!1C is %d\n", c);
            if(c > 7000){
//            	if(guest_current_vmid()==0)
            		changeGuestMode(37, current_regs) ;
//            printH("cDDDDDDDDDDDDDDDDDDDDDDDDDDDD is 1000\n");
            if(c > 12000) {
            		c = 7000;
//            	regs->pc = 0x80000000;
//            	regs->cpsr = regs->cpsr & ~(0x1 << 5);
//            	regs->cpsr = regs->cpsr | (0x1 << 7);
//            	regs->cpsr = regs->cpsr | (0x1 << 8);
//            //    target->regs.cpsr = target->regs.cpsr & ~(0x1F);
//
//            	regs->cpsr = regs->cpsr & ~(0x1F);
//            	regs->cpsr = regs->cpsr | 0x13;
//
//            	c = 0;
////            	c =7000;
            }
            }
            c++;
        }
    } else
        printh("interrupt:no pending irq:%x\n", irq);
}

hvmm_status_t interrupt_save(vmid_t vmid)
{
    hvmm_status_t ret = HVMM_STATUS_UNKNOWN_ERROR;

    /* guest_interrupt_save() */
    if (_guest_ops->save)
        ret = _guest_ops->save(vmid);

    return ret;
}

hvmm_status_t interrupt_restore(vmid_t vmid)
{
    hvmm_status_t ret = HVMM_STATUS_UNKNOWN_ERROR;

    /* guest_interrupt_restore() */
    if (_guest_ops->restore)
        ret = _guest_ops->restore(vmid);

    return ret;
}

hvmm_status_t interrupt_init(struct guest_virqmap *virqmap)
{
    hvmm_status_t ret = HVMM_STATUS_UNKNOWN_ERROR;
    uint32_t cpu = smp_processor_id();

    if (!cpu) {
        _host_ops = _interrupt_module.host_ops;
        _guest_ops = _interrupt_module.guest_ops;

        _guest_virqmap = virqmap;
    }

    /* host_interrupt_init() */
    if (_host_ops->init) {
        ret = _host_ops->init();
        if (ret)
            printh("host initial failed:'%s'\n", _interrupt_module.name);
    }
    _host_ops->enable(39);
    _host_ops->enable(38);
    /* guest_interrupt_init() */
//    if (_guest_ops->init) {
//        ret = _guest_ops->init();
//        if (ret)
//            printh("guest initial failed:'%s'\n", _interrupt_module.name);
//    }

    return ret;
}
