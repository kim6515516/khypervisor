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
}
static struct guest_struct target;
void changeGuestMode(int irq, void *current_regs)
{
	struct arch_regs *regs = (struct arch_regs *)current_regs;
    int i = 0;
	hvmm_status_t ret = HVMM_STATUS_SUCCESS;
    guest_hw_dump_extern(0x4, current_regs);
    printH("changeGuest mode IRQ : %d\n", irq);
    uint32_t lr, sp;

    context_save_banked(&target.context);
    target.vmid = 0;
    target.regs.lr = regs->lr;
    target.regs.pc = regs->pc;
    target.regs.cpsr = regs->cpsr;

    for (i=0; i<ARCH_REGS_NUM_GPR; i++) {
    	target.regs.gpr[i] = regs->gpr[i];
    }
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


    target.context.regs_banked.spsr_irq = target.regs.cpsr;
    target.regs.cpsr = target.regs.cpsr & ~(0x1 << 5);
    target.regs.cpsr = target.regs.cpsr | (0x1 << 7);
//    target->regs.cpsr = target->regs.cpsr & ~(0x1F);
    printH("changeGuest before_cpsr : %x\n", target.regs.cpsr);
    target.regs.cpsr = target.regs.cpsr & ~(0x1F);
    target.regs.cpsr = target.regs.cpsr | 0x12;
    printH("changeGuest after_cpsr : %x\n", target.regs.cpsr);
    printH("changeGuest before_pc : %x\n", target.regs.pc);
    target.context.regs_banked.lr_irq = target.regs.pc - 0x4;
    target.regs.pc = 0xffff0018;
//    target->regs.gpr[13] = 0x80B55FA8;
    target.regs.lr = target.regs.pc - 4;
//    target->regs.pc = 0x8000DA00;
    printH("changeGuest after_pc : %x\n", target.regs.pc);
    printH("changeGuest cpsr : %x\n", target.regs.cpsr);
//    _guest_ops->end(irq);
//    guest_switchto(0, 0);

    volatile int *addr;
    addr = (0x2C002000 + 0x20);
//    *addr = 34;
    printH("changeGuest iar : %x\n", *addr);
    printH("changeGuest end.\n");
    perform_switch_forced2(&target, 0);
//    _mon_switch_to_guest_context(&target->regs);
}
void interrupt_service_routine(int irq, void *current_regs, void *pdata)
{
    struct arch_regs *regs = (struct arch_regs *)current_regs;
    uint32_t cpu = smp_processor_id();

    if(irq != 26 && irq !=30) {
    // 	_guest_ops->end(irq);
    	changeGuestMode(irq, current_regs) ;
    	return;
    }

//    printH("isr IRQ : %d\n", irq);

    if(cpu) {
    	printH("second cpu isr.\n");
    	return ;
    }

    if (irq < MAX_IRQS) {
        if (interrupt_check_guest_irq(irq) == GUEST_IRQ) {

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
            _guest_ops->end(irq);
            interrupt_inject_enabled_guest(NUM_GUESTS_STATIC, irq);
        } else {
            /* host irq */
            if (irq < MAX_PPI_IRQS) {
                if (_host_ppi_handlers[cpu][irq])
                    _host_ppi_handlers[cpu][irq](irq, regs, 0);
            } else {
                if (_host_spi_handlers[irq])
                    _host_spi_handlers[irq](irq, regs, 0);
            }
            /* host_interrupt_end() */
            _host_ops->end(irq);
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

    /* guest_interrupt_init() */
//    if (_guest_ops->init) {
//        ret = _guest_ops->init();
//        if (ret)
//            printh("guest initial failed:'%s'\n", _interrupt_module.name);
//    }

    return ret;
}
