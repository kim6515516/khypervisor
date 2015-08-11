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
    printH("enter changeGuest mode IRQ : %d\n", irq);
    uint32_t lr, sp;
    target = get_guest_pointer(guest_current_vmid());
    if (target)
    	;
    else
    	printH("target is null : %d\n", irq);

    ci  = vdev_find_tag(0, 55);
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

    target->context.regs_banked.spsr_irq = target->regs.cpsr;
    target->regs.cpsr = target->regs.cpsr & ~(0x1 << 5);
    target->regs.cpsr = target->regs.cpsr | (0x1 << 7);
//    target->regs.cpsr = target->regs.cpsr & ~(0x1F);
    printH("changeGuest before_cpsr : %x\n", target->regs.cpsr);
    target->regs.cpsr = target->regs.cpsr & ~(0x1F);
    target->regs.cpsr = target->regs.cpsr | 0x12;
    printH("changeGuest after_cpsr : %x\n", target->regs.cpsr);
    printH("changeGuest before_pc : %x\n", target->regs.pc);
    printH("changeGuest elr_hyp : %x\n", target->context.regs_banked.elr_hyp);
    target->context.regs_banked.lr_irq = target->regs.pc + 4;

//    if (guest_current_vmid() == 0)  // linux
//    	target->regs.pc = 0xffff0018;
//    else // bm guest
    	target->regs.pc = 0xffff0038;
//    target->regs.gpr[13] = 0x80B55FA8;
//    target->regs.lr = target->regs.pc - 4;
//    target->regs.pc = 0x8000DA00;
    printH("changeGuest after_pc : %x\n", target->regs.pc);
    printH("changeGuest cpsr : %x\n", target->regs.cpsr);
//    _guest_ops->end(irq);
//    guest_switchto(0, 0);

    volatile int *addr;
    addr = (0x2C002000 + 0x20);
//    *addr = 34;
    printH("changeGuest iar : %x\n", *addr);


    printH("find dev : %d \n", ci);
    vdev_execute(0, ci, 1, irq); //irq inject
    printH("changeGuest. switch forced\n");
    perform_switch_forced2(target, 0);
    regs->cpsr = target->regs.cpsr;
    regs->lr = target->regs.lr;
    regs->pc = target->regs.pc;
    printH("changeGuest. end\n");
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


void interrupt_service_routine(int irq, void *current_regs, void *pdata)
{
    struct arch_regs *regs = (struct arch_regs *)current_regs;
    uint32_t cpu = smp_processor_id();
    uint32_t rx = 0;
//    c ++;
//    if(( irq != 26) || (irq ==30 && c > 10000) ) {
//    // 	_guest_ops->end(irq);
//    		changeGuestMode(irq, current_regs) ;
//    	return;
//    }

    if(irq == 0xffffffff)
    {

        rx=GET32(CLO);
        rx += INTERVAL;
        PUT32(C3,rx);

    	if( _guest_module.ops->init)
    		guest_switchto(sched_policy_determ_next(), 0);


    } else
    	 printH("isr IRQ !!!!!: %x\n", irq);


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
