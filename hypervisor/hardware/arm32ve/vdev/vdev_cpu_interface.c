#include <vdev.h>
#define DEBUG
#include <log/print.h>

//gicc
#define CPU_INTERFACE_BASE_ADDR 0x2C002000

#define GIC_OFFSET_GICC_CTLR 0x0000
#define GIC_OFFSET_GICC_PMR 0x0004
#define GIC_OFFSET_GICC_BPR 0x0008
#define GIC_OFFSET_GICC_IAR 0x000C
#define GIC_OFFSET_GICC_EOIR 0x0010
#define GIC_OFFSET_GICC_RPR 0x0014
#define GIC_OFFSET_GICC_HPPIR 0x0018
#define GIC_OFFSET_GICC_ABPR 0x001C
#define GIC_OFFSET_GICC_AIAR 0x0020
#define GIC_OFFSET_GICC_AEOIR 0x0024
#define GIC_OFFSET_GICC_AHPPIR 0x0028
#define GIC_OFFSET_GICC_APRN 0x00D0
#define GIC_OFFSET_GICC_NSAPRN 0x00E0
#define GIC_OFFSET_GICC_IIDR 0x00FC
#define GIC_OFFSET_GICC_DIR 0x1000



#define PENDING_MAX 50

int queue[NUM_GUESTS_STATIC][PENDING_MAX];
int front[NUM_GUESTS_STATIC], rear[NUM_GUESTS_STATIC];

void init_queue(int vmn) {
	front[vmn] = rear[vmn] = 0;
}

void clear_queue(int vmn) {
	front[vmn] = rear[vmn];
}

int put(int k, int vmn) {
	if ((rear[vmn] + 1) % PENDING_MAX == front[vmn]) {
//		printH("\n   Queue overflow.");
		return -1;
	}
	queue[vmn][rear[vmn]] = k;
	rear[vmn] = ++rear[vmn] % PENDING_MAX;
	return k;
}
int is_get(int vmn) {
	if (front[vmn] == rear[vmn]) {
//		printH("\n   Queue underflow.");
		return -1;
	} else
		return 1;
}
int get(int vmn) {
	int i;
	if (front[vmn] == rear[vmn]) {
//		printH("\n   Queue underflow.");
		return -1;
	}
	i = queue[vmn][front[vmn]];
	front[vmn] = ++front[vmn] % PENDING_MAX;
	return i;
}

void print_queue(int vmn) {
	int i;
	printH("\n   Queue contents : Front -----> Rear\n");
	for (i = front[vmn]; i != rear[vmn]; i = ++i % PENDING_MAX)
		printH("%d ", queue[vmn][i]);
	printH("\n");
}
static int countPending[NUM_GUESTS_STATIC] ;











struct cpu_interface_regs {
    uint32_t GICC_CTLR; /* 0x00  CPU Interface Control Register */
    uint32_t GICC_PMR; /* 0x04  Interrupt Priority Mask Register */

    uint32_t GICC_BPR; /* 0x08  Binary Point Register */
    uint32_t GICC_IAR; /* 0x0C  Interrupt Acknowledge Register */
    uint32_t GICC_EOIR; /* 0x10  End of Interrupt Register   */

    uint32_t GICC_RPR; /* 0x14  Running Priority Register */
    uint32_t GICC_HPPIR; /* 0x18  Highest Priority Pending Interrupt Register */
    uint32_t GICC_ABPR; /* 0x1C  Aliased Binary Point Register */
    uint32_t GICC_AIAR; /* 0x20  Aliased Interrupt Acknowledge Register */
    uint32_t GICC_AEOIR; /* 0x24  Aliased End of Interrupt Register */
    uint32_t GICC_AHPPIR; /* 0x28  Aliased Highest Priority Pending Interrupt Register */

    uint32_t GICC_APRn[12]; /* 0x00D0-0x00DC  Active Priorities Registers */
    uint32_t GICC_NSAPRn[12]; /* 0x00E0-0x00EC   Non-secure Active Priorities Registers */

    uint32_t GICC_IIDR; /* 0x00FC CPU Interface Identification Register */
    uint32_t GICC_DIR; /* 0x1000  Deactivate Interrupt Register */
};

struct cpu_interface_handler_entry {
    uint32_t offset;
    vdev_callback_t handler;
};

static struct cpu_interface_regs ci_regs[NUM_GUESTS_STATIC];
static struct cpu_interface_regs pending_ci_regs[NUM_GUESTS_STATIC][PENDING_MAX];



static struct vdev_memory_map _vdev_cpu_interface_info = {
   .base = CPU_INTERFACE_BASE_ADDR,
   .size = 1000,
};


static hvmm_status_t handler_null(uint32_t write, uint32_t offset,
        uint32_t *pvalue, enum vdev_access_size access_size)
{
	printH("cpu interface handler_null\n");
	return HVMM_STATUS_BAD_ACCESS;
}
static hvmm_status_t handler_F(uint32_t write, uint32_t offset,
        uint32_t *pvalue, enum vdev_access_size access_size)
{
	return HVMM_STATUS_BAD_ACCESS;
}
static hvmm_status_t handler_E(uint32_t write, uint32_t offset,
        uint32_t *pvalue, enum vdev_access_size access_size)
{
	return HVMM_STATUS_BAD_ACCESS;
}
static hvmm_status_t handler_D(uint32_t write, uint32_t offset,
        uint32_t *pvalue, enum vdev_access_size access_size)
{
	return HVMM_STATUS_BAD_ACCESS;
}
static hvmm_status_t handler_2(uint32_t write, uint32_t offset,
        uint32_t *pvalue, enum vdev_access_size access_size)
{
	return HVMM_STATUS_BAD_ACCESS;
}
static hvmm_status_t handler_1(uint32_t write, uint32_t offset,
        uint32_t *pvalue, enum vdev_access_size access_size)
{
	return HVMM_STATUS_BAD_ACCESS;
}

static hvmm_status_t handler_0(uint32_t write, uint32_t offset,
        uint32_t *pvalue, enum vdev_access_size access_size)
{
	/* CTLR, PMR, BPR, IAR */
    hvmm_status_t result = HVMM_STATUS_BAD_ACCESS;
    vmid_t vmid = guest_current_vmid();
    struct cpu_interface_regs *regs = &ci_regs[vmid];
    uint32_t woffset = offset / 4;
    switch (woffset) {
    case GIC_OFFSET_GICC_CTLR: /* RW */
        if (write)
            regs->GICC_CTLR = *pvalue;
        else
            *pvalue = regs->GICC_CTLR;
        result = HVMM_STATUS_SUCCESS;
        break;
    case GIC_OFFSET_GICC_PMR: /* RO */
        if (write) {
            regs->GICC_PMR = *pvalue;
        } else
        	*pvalue = regs->GICC_PMR;

        result = HVMM_STATUS_SUCCESS;
        break;
    case GIC_OFFSET_GICC_BPR: /* RO */
        if (write) {
            regs->GICC_BPR = *pvalue;
        } else
        	*pvalue = regs->GICC_BPR;
        result = HVMM_STATUS_SUCCESS;
        break;
    case GIC_OFFSET_GICC_IAR:
        if (write) {
            regs->GICC_IAR = *pvalue;
        } else
        	*pvalue = regs->GICC_IAR;
        result = HVMM_STATUS_SUCCESS;
    	break;
//    default: { /* RW GICD_IGROUPR */
//        int igroup = woffset - GICD_IGROUPR;
//        if (igroup >= 0 && igroup < VGICD_NUM_IGROUPR) {
//            if (write)
//                regs->IGROUPR[igroup] = *pvalue;
//            else
//                *pvalue = regs->IGROUPR[igroup];
//
//            result = HVMM_STATUS_SUCCESS;
//        }
//    }
//        break;
    }
    if (result != HVMM_STATUS_SUCCESS)
        printH("vgicd: invalid access offset:%x write:%d\n", offset,
                write);

    return result;
}
static struct cpu_interface_handler_entry _ci_handler_map[0x10] = {
/* 0x00 ~ 0x0F */
{ 0x00, handler_0 }, /* CTLR, PMR, BPR, IAR */
{ 0x01, handler_1 }, /* EOIR, RPR, HPPIR, ABPR */
{ 0x02, handler_2 }, /* AIAR, AEOIR, AHPPIR */
{ 0x03, handler_null},
{ 0x04, handler_null},
{ 0x05, handler_null},
{ 0x06, handler_null},
{ 0x07, handler_null},
{ 0x08, handler_null},
{ 0x09, handler_null},
{ 0x0A, handler_null},
{ 0x0B, handler_null},
{ 0x0C, handler_null},
{ 0x0D, handler_D}, /* APRN */
{ 0x0E, handler_E }, /* NSAPRN */
{ 0x0F, handler_F }, /* IIDR */
//{ 0x0F, handler_F }, /* IIDR */
};
//static hvmm_status_t vdev_cpu_interface_access_handler(uint32_t write, uint32_t offset,
//        uint32_t *pvalue, enum vdev_access_size access_size)
//{
//    printH("%s: %s offset:%x value:%x access_size : %d\n", __func__,
//            write ? "write" : "read", offset,
//            write ? *pvalue : (uint32_t) pvalue, access_size);
//    hvmm_status_t result = HVMM_STATUS_BAD_ACCESS;
//
//
//    // DIR
//
//    uint8_t offsetidx = (uint8_t) ((offset & 0xF0) >> 4);
//    printH("offset_idx : %d\n", offsetidx);
//    result = _ci_handler_map[offsetidx].handler(write, offset, pvalue,
//            access_size);
//
//    return result;
//}

static hvmm_status_t vdev_cpu_interface_access_handler(uint32_t write, uint32_t offset,
        uint32_t *pvalue, enum vdev_access_size access_size)
{

    hvmm_status_t result = HVMM_STATUS_BAD_ACCESS;
    unsigned int vmid = guest_current_vmid();
    volatile int *addr;
    addr = (CPU_INTERFACE_BASE_ADDR + offset);
    volatile int *daddr = (CPU_INTERFACE_BASE_ADDR + GIC_OFFSET_GICC_DIR);

    if (!write) {
        /* READ */
    	*pvalue =  (uint32_t) (*((volatile unsigned int*) (CPU_INTERFACE_BASE_ADDR + offset)));

//        if (offset == 20)
//        	*pvalue = 34;
    	if(offset == GIC_OFFSET_GICC_AIAR) {
    		printH("AIAR CALL!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    	}
        if (offset == GIC_OFFSET_GICC_IAR) {
        	*pvalue = ci_regs[vmid].GICC_IAR;
//        	if( vmid == 1)
//        		printH("vm: %d read iar: GICC_IAR %d\n", vmid, *pvalue);
        }
//    	printH("1111111111111 %x\n", *pvalue);
       } else {
        /* WRITE */
        	if (offset == GIC_OFFSET_GICC_EOIR) { // EOIR
//        		ci_regs[0].GICC_IAR = -1;
//        		if(vmid == 1)
//       		printH("vm:%d write eoir : ci_regs[0].GICC_IAR = 0x000003FF  %d\n",vmid, *pvalue);

        		ci_regs[vmid].GICC_IAR = 0x000003FF ;
//        		ci_regs[0].GICC_IAR = *pvalue ;
//        		if (*pvalue == 37) {
//        			if(vmid == 0)
//        				*addr = 38;
//        			if(vmid ==1)
//        				*addr = 39;
//        		} else if ( *pvalue == 34){
//        			if(vmid == 0)
//        				*addr = 34;
//        			if(vmid ==1)
//        				*addr = 35;
//
//        		}else
//        			*addr = *pvalue;

        		int pirq = interrupt_virq_to_pirq(vmid, *pvalue);
        		if(pirq != PIRQ_INVALID)
        			*addr = pirq;
        		else
        			*addr = *pvalue;
//        		*daddr = *pvalue;
//                int *dir = 0x2c001000;
//                *dir = *pvalue;

        	}
        	else
        		*addr = *pvalue;

//        	ci_regs[0].GICC_IAR = (*((volatile unsigned int*) (CPU_INTERFACE_BASE_ADDR + offset)));

    }
//    printH("%s: %s offset:%x value:%x\n", __func__,
//            write ? "write" : "read", offset,
//            write ? *pvalue : *pvalue);
//    printH("AAAAAAAAAAAAAA %x\n", *addr);
    result = HVMM_STATUS_SUCCESS;
    return result;
}
static hvmm_status_t vdev_cpu_interface_read(struct arch_vdev_trigger_info *info,
                        struct arch_regs *regs)
{
    uint32_t offset = info->fipa - _vdev_cpu_interface_info.base;

    return vdev_cpu_interface_access_handler(0, offset, info->value, info->sas);
}

static hvmm_status_t vdev_cpu_interface_write(struct arch_vdev_trigger_info *info,
                        struct arch_regs *regs)
{
    uint32_t offset = info->fipa - _vdev_cpu_interface_info.base;
//    printH("PC:%x, CPSR:%x\n", regs->pc, regs->cpsr);
    return vdev_cpu_interface_access_handler(1, offset, info->value, info->sas);
}

static int32_t vdev_cpu_interface_post(struct arch_vdev_trigger_info *info,
                        struct arch_regs *regs)
{
    uint8_t isize = 4;

    if (regs->cpsr & 0x20) /* Thumb */
        isize = 2;

    regs->pc += isize;

    return 0;
}

static int32_t vdev_cpu_interface_check(struct arch_vdev_trigger_info *info,
                        struct arch_regs *regs)
{
    uint32_t offset = info->fipa - _vdev_cpu_interface_info.base;

    if (info->fipa >= _vdev_cpu_interface_info.base &&
        offset < _vdev_cpu_interface_info.size)
        return 0;
    /**
     * Find vdev using tag.  Hard coding.
     */
    if( !regs ) {
    	return 55;
    }
    return VDEV_NOT_FOUND;
}




static hvmm_status_t vdev_cpu_interface_reset(void)
{


    printH("vdev init:'%s'\n", __func__);
    int j = 0;
    int i = 0;

    for (i = 0; i < NUM_GUESTS_STATIC; i++) {

    	ci_regs[i].GICC_CTLR =
                (uint32_t) (*((volatile unsigned int*) (CPU_INTERFACE_BASE_ADDR
                        + GIC_OFFSET_GICC_CTLR)));
    	ci_regs[i].GICC_PMR =
                (uint32_t) (*((volatile unsigned int*) (CPU_INTERFACE_BASE_ADDR
                        + GIC_OFFSET_GICC_PMR)));

    	ci_regs[i].GICC_BPR =
                (uint32_t) (*((volatile unsigned int*) (CPU_INTERFACE_BASE_ADDR
                        + GIC_OFFSET_GICC_BPR)));
//    	ci_regs[i].GICC_IAR =
//                (uint32_t) (*((volatile unsigned int*) (CPU_INTERFACE_BASE_ADDR
//                        + GIC_OFFSET_GICC_IAR)));
    	ci_regs[i].GICC_IAR = 0x000003FF;

    	ci_regs[i].GICC_EOIR =
                (uint32_t) (*((volatile unsigned int*) (CPU_INTERFACE_BASE_ADDR
                        + GIC_OFFSET_GICC_EOIR)));
    	ci_regs[i].GICC_RPR =
                (uint32_t) (*((volatile unsigned int*) (CPU_INTERFACE_BASE_ADDR
                        + GIC_OFFSET_GICC_RPR)));
    	ci_regs[i].GICC_HPPIR =
                (uint32_t) (*((volatile unsigned int*) (CPU_INTERFACE_BASE_ADDR
                        + GIC_OFFSET_GICC_HPPIR)));
    	ci_regs[i].GICC_ABPR =
                (uint32_t) (*((volatile unsigned int*) (CPU_INTERFACE_BASE_ADDR
                        + GIC_OFFSET_GICC_ABPR)));
    	ci_regs[i].GICC_AIAR =
                (uint32_t) (*((volatile unsigned int*) (CPU_INTERFACE_BASE_ADDR
                        + GIC_OFFSET_GICC_AIAR)));

    	ci_regs[i].GICC_AEOIR =
                (uint32_t) (*((volatile unsigned int*) (CPU_INTERFACE_BASE_ADDR
                        + GIC_OFFSET_GICC_AEOIR)));
    	ci_regs[i].GICC_AHPPIR =
                (uint32_t) (*((volatile unsigned int*) (CPU_INTERFACE_BASE_ADDR
                        + GIC_OFFSET_GICC_AHPPIR)));

        for(j = 0; j < 12; j++){
        	ci_regs[i].GICC_APRn[j] = 0x00000000;
//                (uint32_t) (*((volatile unsigned int*) (CPU_INTERFACE_BASE_ADDR
//                        + GIC_OFFSET_GICC_APRN)));
        	ci_regs[i].GICC_NSAPRn[j]= 0x00000000;
//                (uint32_t) (*((volatile unsigned int*) (CPU_INTERFACE_BASE_ADDR
//                        + GIC_OFFSET_GICC_NSAPRN)));

        }
        ci_regs[i].GICC_IIDR =
                (uint32_t) (*((volatile unsigned int*) (CPU_INTERFACE_BASE_ADDR
                        + GIC_OFFSET_GICC_IIDR)));
        ci_regs[i].GICC_DIR =
                (uint32_t) (*((volatile unsigned int*) (CPU_INTERFACE_BASE_ADDR
                        + GIC_OFFSET_GICC_DIR)));
    }
    return HVMM_STATUS_SUCCESS;
}
int isAdaptPending[NUM_GUESTS_STATIC];
static hvmm_status_t vdev_cpu_interface_execute(int level, int num, int type, int irq)
{
	volatile int *addr;

	unsigned int vmn = guest_current_vmid();
	hvmm_status_t result = HVMM_STATUS_BAD_ACCESS;

//	 printH("vdev_cpu_interface_execute: irq: %d, type : %d\n", irq, type);
	if (type == 0) { // EOI
	    addr = (CPU_INTERFACE_BASE_ADDR + GIC_OFFSET_GICC_EOIR);
	    *addr = irq;
//        int *dir = 0x2c001000;
//              *dir = irq;

	    printH("vdev_cpu_interface_execute, eoi: irq: %d\n", irq);
	    return HVMM_STATUS_SUCCESS;

	} else if (type == 1) {  // inject
		if(isAdaptPending[vmn] == 1)
		{
			isAdaptPending[vmn]=0;
			return HVMM_STATUS_SUCCESS;
		}

		if(ci_regs[vmn].GICC_IAR != 0x000003FF) {
			printH("vm:%d vdev_cpu_interface_execute, inject-reject1: irq: %d pen: %d\n", vmn, irq, ci_regs[vmn].GICC_IAR );
			ci_regs[vmn].GICC_IAR = irq;

			return HVMM_STATUS_SUCCESS;
		} else {
		ci_regs[vmn].GICC_IAR = irq;


//		if(vmn == 1)
//			printH("vm:%d vdev_cpu_interface_execute, inject-ok: irq: %d\n", vmn, irq);
		}
		return HVMM_STATUS_SUCCESS;
	} else if (type == 2) {
		return ci_regs[vmn].GICC_IAR;

	} else if (type == 3) { //add pending irq to myself


//		int i = 0;
//		if(countPending[vmn] > PENDING_MAX){
//			printH("overflow pending interrupt\n");
//			for (i = 0; i< PENDING_MAX; i++)
//				printH("vm:%d [%d] %d\n",vmn, i,  pending_ci_regs[vmn][i].GICC_IAR);
//		}
//
////		printH("cur vm:%d, ", vmn);
////		printH("target vm:%d Add pending irq %d \n", vmn, irq);
//		pending_ci_regs[vmn][countPending[vmn]].GICC_IAR = irq;
//		countPending[vmn]++;

		put(irq, vmn);
		return HVMM_STATUS_SUCCESS;

	} else if (type == 4) { //adapt pending irq


//		countPending[vmn]--;
//
//		ci_regs[vmn].GICC_IAR = pending_ci_regs[vmn][countPending[vmn]].GICC_IAR;
//		printH("vm:%d pop pending irq %d \n", vmn, pending_ci_regs[vmn][countPending[vmn]].GICC_IAR);
//		isAdaptPending[vmn] = 1;
//		return pending_ci_regs[vmn][countPending[vmn]].GICC_IAR;

		isAdaptPending[vmn] = 1;
		return get(vmn);

	} else if (type == 5) {
		vmn = irq;
//		return countPending[vmn];
		return is_get(vmn);

	} else if (type == 6) {
//		printH("cur vm:%d, ", vmn);
		if(vmn ==0)
			vmn = 1;
		else if(vmn ==1)
			vmn = 0;

//		int i = 0;
//		if(countPending[vmn] > PENDING_MAX){
//			printH("overflow pending interrupt\n");
//			for (i = 0; i< PENDING_MAX; i++)
//				printH("vm:%d [%d] %d\n",vmn, i,  pending_ci_regs[vmn][i].GICC_IAR);
//		}
//		static struct cpu_interface_regs ci_regs[NUM_GUESTS_STATIC];
//		static struct cpu_interface_regs pending_ci_regs[NUM_GUESTS_STATIC][PENDING_MAX];
//		static int countPending[NUM_GUESTS_STATIC] ;

//		printH("target vm:%d Add pending irq %d \n", vmn, irq);
//		pending_ci_regs[vmn][countPending[vmn]].GICC_IAR = irq;
//		countPending[vmn]++;
//	    addr = (CPU_INTERFACE_BASE_ADDR + GIC_OFFSET_GICC_HPPIR);
//	    *addr = irq;
		put(irq, vmn);
		return HVMM_STATUS_SUCCESS;
	} else if (type == 7) {
		vmn = irq;
//		int i =0 ;
//			for (i = 0; i< countPending[vmn]; i++)
//				printH("vm:%d [%d] %d\n",vmn, i,  pending_ci_regs[vmn][i].GICC_IAR);
//
//		countPending[vmn]--;
//
//		ci_regs[vmn].GICC_IAR = pending_ci_regs[vmn][countPending[vmn]].GICC_IAR;
////		printH("vm:%d pop pending irq %d \n", vmn, pending_ci_regs[vmn][countPending[vmn]].GICC_IAR);
////		isAdaptPending[vmn] = 1;
//		return pending_ci_regs[vmn][countPending[vmn]].GICC_IAR;
//		print_queue(vmn);
		return get(vmn);
//		return HVMM_STATUS_SUCCESS;

	} else if (type == 8) {  // inject
		int cur_vm = guest_current_vmid();
		vmn = (cur_vm + 1) % 2 ;
//		if(isAdaptPending[vmn] == 1)
//		{
//			isAdaptPending[vmn]=0;
//			return HVMM_STATUS_SUCCESS;
//		}

		if(ci_regs[vmn].GICC_IAR != 0x000003FF) {
			ci_regs[vmn].GICC_IAR = irq;
//			printH("next vm:%d vdev_cpu_interface_execute, inject-reject8: irq: %d\n", vmn, irq);
			return HVMM_STATUS_SUCCESS;
		} else {
		ci_regs[vmn].GICC_IAR = irq;


//		if(irq != 34)
//			printH("next vm:%d vdev_cpu_interface_execute, inject-ok8: irq: %d\n", vmn, irq);
		}
		return HVMM_STATUS_SUCCESS;
	}

	return HVMM_STATUS_SUCCESS;
}

struct vdev_ops _vdev_cpu_interface_ops = {
    .init = vdev_cpu_interface_reset,
    .check = vdev_cpu_interface_check,
    .read = vdev_cpu_interface_read,
    .write = vdev_cpu_interface_write,
    .post = vdev_cpu_interface_post,
    .execute = vdev_cpu_interface_execute,
};

struct vdev_module _vdev_cpu_interface_module = {
    .name = "K-Hypervisor vDevice cpu_interface Module",
    .author = "Kookmin Univ.",
    .ops = &_vdev_cpu_interface_ops,
    .tag = 55,
};

hvmm_status_t vdev_cpu_interface()
{
    hvmm_status_t result = HVMM_STATUS_BUSY;

    result = vdev_register(VDEV_LEVEL_LOW, &_vdev_cpu_interface_module);
    if (result == HVMM_STATUS_SUCCESS)
        printH("vdev registered:'%s'\n", _vdev_cpu_interface_module.name);
    else {
        printh("%s: Unable to register vdev:'%s' code=%x\n",
                __func__, _vdev_cpu_interface_module.name, result);
    }

    return result;
}
vdev_module_low_init(vdev_cpu_interface);
