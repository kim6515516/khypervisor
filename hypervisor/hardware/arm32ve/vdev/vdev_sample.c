#include <vdev.h>
#define DEBUG
#include <log/print.h>

#define SAMPLE_BASE_ADDR 0x40000000

struct vdev__local_base_regs {
    uint32_t irq_pending0;
};



static struct vdev_memory_map _vdev_sample_info = {
   .base = SAMPLE_BASE_ADDR,
   .size = 0x10000,
};

static struct vdev__local_base_regs sregs[NUM_GUESTS_STATIC];

static hvmm_status_t vdev_sample_access_handler(uint32_t write, uint32_t offset,
        uint32_t *pvalue, enum vdev_access_size access_size)
{

    hvmm_status_t result = HVMM_STATUS_BAD_ACCESS;
    unsigned int vmid = guest_current_vmid();
    if (!write) {
        /* READ */
        switch (offset) {

        case 0x60:

        	*pvalue = sregs[0].irq_pending0;
//        	printH("send:%x\n",  sregs[0].irq_pending0);
        	*(int*)(SAMPLE_BASE_ADDR + offset) = 0;
        	sregs[0].irq_pending0 = 0;

        	break;
        default:
        	*pvalue = *(int*)(SAMPLE_BASE_ADDR + offset);
        	break;
        }

    	result = HVMM_STATUS_SUCCESS;
    } else {
        /* WRITE */
        switch (offset) {
//        case 0x40:
        case 0x44:
        case 0x48:
        case 0x4C:
        	printH("NOT CPU 234 local timer init contros.\n");
            break;
        default:
        	 *(int*)(SAMPLE_BASE_ADDR + offset) = *pvalue;
        	break;
        }

        result = HVMM_STATUS_SUCCESS;
    }

//    printH("%s: %s offset:%x value:%x, size:%x\n", __func__,
//            write ? "write" : "read", offset,
//            write ? *pvalue : *pvalue, access_size);

    return result;
}

static hvmm_status_t vdev_sample_read(struct arch_vdev_trigger_info *info,
                        struct arch_regs *regs)
{
    uint32_t offset = info->fipa - _vdev_sample_info.base;

    return vdev_sample_access_handler(0, offset, info->value, info->sas);
}

static hvmm_status_t vdev_sample_write(struct arch_vdev_trigger_info *info,
                        struct arch_regs *regs)
{
    uint32_t offset = info->fipa - _vdev_sample_info.base;

    return vdev_sample_access_handler(1, offset, info->value, info->sas);
}

static int32_t vdev_sample_post(struct arch_vdev_trigger_info *info,
                        struct arch_regs *regs)
{
    uint8_t isize = 4;

    if (regs->cpsr & 0x20) /* Thumb */
        isize = 2;
//    printH("pc: %x, lr: %x cpsr: %x gp7:%x\n", regs->pc, regs->lr, regs->cpsr, regs->gpr[7]);

    	regs->pc += isize;

    return 0;
}


static hvmm_status_t vdev_ic_rpi2_execute(int level, int num, int type, int data)
{
	volatile int *addr;
	hvmm_status_t result = HVMM_STATUS_BAD_ACCESS;

//	 printH("vdev_ic_rpi2_execute: irq: %d, type : %d\n", irq, type);
	if (type == 0) { // EOI

	    return HVMM_STATUS_SUCCESS;

	} else if (type == 1) {  // inject

//		printH("rev :%x\n", data);
		sregs[0].irq_pending0 = data;//0x8;//data;
//		printH("vdev_ic_rpi2_execute, IC_PENDING1 inject-ok: irq: %d\n", irq);


	} else if (type == 2) {

//		printH("vdev_ic_rpi2_execute, IC_PENDING2 inject-ok: irq: %d\n", irq);


	} else if (type == 3) { //all coply
//		printH("All Copy IC_pri2\n");


		return HVMM_STATUS_SUCCESS;
	}
	else if (type == 4) { //all coply
	//		printH("All Copy IC_pri2\n");

			return HVMM_STATUS_SUCCESS;
		}
	return HVMM_STATUS_SUCCESS;
}

static int32_t vdev_sample_check(struct arch_vdev_trigger_info *info,
                        struct arch_regs *regs)
{
    uint32_t offset = info->fipa - _vdev_sample_info.base;

//    printH("dev_smaple_check_info fipa : %x\n", info->fipa);
    if (info->fipa >= _vdev_sample_info.base &&
        offset < _vdev_sample_info.size)
        return 0;

    /**
     * Find vdev using tag.  Hard coding.
     */
    if( !regs ) {
    	return 77;
    }

    return VDEV_NOT_FOUND;
}

static hvmm_status_t vdev_sample_reset(void)
{
    printH("vdev init:'%s'\n", __func__);
    return HVMM_STATUS_SUCCESS;
}

struct vdev_ops _vdev_sample_ops = {
    .init = vdev_sample_reset,
    .check = vdev_sample_check,
    .read = vdev_sample_read,
    .write = vdev_sample_write,
    .post = vdev_sample_post,
    .execute = vdev_ic_rpi2_execute,
};

struct vdev_module _vdev_sample_module = {
    .name = "K-Hypervisor vDevice Sample Module",
    .author = "Kookmin Univ.",
    .ops = &_vdev_sample_ops,
    .tag = 77,
};

hvmm_status_t vdev_sample_init()
{
    hvmm_status_t result = HVMM_STATUS_BUSY;

    result = vdev_register(VDEV_LEVEL_LOW, &_vdev_sample_module);
    if (result == HVMM_STATUS_SUCCESS)
        printH("vdev registered:'%s'\n", _vdev_sample_module.name);
    else {
        printH("%s: Unable to register vdev:'%s' code=%x\n",
                __func__, _vdev_sample_module.name, result);
    }

    return result;
}
vdev_module_low_init(vdev_sample_init);
