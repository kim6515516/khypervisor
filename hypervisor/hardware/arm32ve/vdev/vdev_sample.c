#include <vdev.h>
#define DEBUG
#include <log/print.h>

#define SAMPLE_BASE_ADDR 0xFFFF0000

struct vdev_sample_regs {
    uint32_t axis_x;
    uint32_t axis_y;
    uint32_t axis_z;
};

static struct vdev_memory_map _vdev_sample_info = {
   .base = SAMPLE_BASE_ADDR,
   .size = 0x1000,
};

static struct vdev_sample_regs sample_regs[NUM_GUESTS_STATIC];

static hvmm_status_t vdev_sample_access_handler(uint32_t write, uint32_t offset,
        uint32_t *pvalue, enum vdev_access_size access_size)
{
    printH("%s: %s offset:%d value:%x\n", __func__,
            write ? "write" : "read", offset,
            write ? *pvalue : (uint32_t) pvalue);
    hvmm_status_t result = HVMM_STATUS_BAD_ACCESS;
    unsigned int vmid = guest_current_vmid();
    if (!write) {
        /* READ */
        switch (offset) {
        case 0x0:
            *pvalue = 0xe24ee004;
            result = HVMM_STATUS_SUCCESS;
            break;
        case 0x4:
            *pvalue = 0xe88d4001;
            result = HVMM_STATUS_SUCCESS;
            break;
        case 0x8:
            *pvalue = 0xe14fe000;
            result = HVMM_STATUS_SUCCESS;
            break;
        case 0xC:
            *pvalue = 0xe58de008;
            result = HVMM_STATUS_SUCCESS;
            break;
        case 0x10:
            *pvalue = 0xe10f0000;
            result = HVMM_STATUS_SUCCESS;
            break;
        case 0x14:
            *pvalue = 0xe2200001;
            result = HVMM_STATUS_SUCCESS;
            break;
        case 0x18:
            *pvalue = 0xe16ff000;
            result = HVMM_STATUS_SUCCESS;
            break;
        case 0x1C:
            *pvalue = 0xe20ee00f;
            result = HVMM_STATUS_SUCCESS;
            break;
        case 0x20:
            *pvalue = 0xe1a0000d;
            result = HVMM_STATUS_SUCCESS;
            break;
        case 0x24:
            *pvalue = 0xe78fe10e;
            result = HVMM_STATUS_SUCCESS;
            break;
        case 0x28:
            *pvalue = 0xe1b0f00e;
            result = HVMM_STATUS_SUCCESS;
            break;
        }
    } else {
        /* WRITE */
        switch (offset) {
        case 0x0:
            sample_regs[vmid].axis_x = *pvalue;
            result = HVMM_STATUS_SUCCESS;
            break;
        case 0x4:
            sample_regs[vmid].axis_y = *pvalue;
            result = HVMM_STATUS_SUCCESS;
            break;
        case 0x8:
            /* read-only register, ignored, but no error */
            result = HVMM_STATUS_SUCCESS;
            break;
        }
    }
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

    regs->pc += isize;

    return 0;
}

static int32_t vdev_sample_check(struct arch_vdev_trigger_info *info,
                        struct arch_regs *regs)
{
    uint32_t offset = info->fipa - _vdev_sample_info.base;

    printH("dev_smaple_check_info fipa : %x\n", info->fipa);
    if (info->fipa >= _vdev_sample_info.base &&
        offset < _vdev_sample_info.size)
        return 0;
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
};

struct vdev_module _vdev_sample_module = {
    .name = "K-Hypervisor vDevice Sample Module",
    .author = "Kookmin Univ.",
    .ops = &_vdev_sample_ops,
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
