#include <linux/module.h>
#include <linux/init.h>
#include <linux/mmzone.h>
//#include <asm/mmzone.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/if_arp.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/kref.h>
#include <linux/kallsyms.h>
#include <asm/pgtable.h>
//#include <asm/pat.h>
#include <asm/tlbflush.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <asm/unistd.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/unistd.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/mutex.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#include "../../include/pim.h"
#include "dma_lib_kernel.h"

struct timespec start_cmd_case, end_cmd_case;
uint64_t diff_cmd_case;

struct timespec start_dsc_clear, end_dsc_clear;
uint64_t diff_dsc_clear;

struct timespec start_dsc_init, end_dsc_init;
uint64_t diff_dsc_init;

struct timespec start_dma_tx, end_dma_tx;
uint64_t diff_dma_tx;

struct timespec start_kern_cpy, end_kern_cpy;
uint64_t diff_kern_cpy;

struct timespec start_SG_MATMUL, end_SG_MATMUL;
uint64_t diff_SG_MATMUL;

struct timespec start_SIM_MATMUL, end_SIM_MATMUL;
uint64_t diff_SIM_MATMUL;

struct timespec start_SIM_MATMUL_PER, end_SIM_MATMUL_PER;
uint64_t diff_SIM_MATMUL_PER;


struct timespec start_read_A, end_read_A;
uint64_t diff_read_A;

struct timespec start_read_B, end_read_B;
uint64_t diff_read_B;

struct timespec start_write_C, end_write_C;
uint64_t diff_write_C;

struct timespec start_conf, end_conf;
uint64_t diff_conf;


static uint64_t vm_time = 0;
static uint64_t ew_add_time = 0;
static uint64_t ew_mul_time = 0;
static uint64_t ew_sub_time = 0;
static uint64_t desc_time = 0;

static DEFINE_MUTEX(dma_lock);

#ifndef INTR_ENABLE
int __attribute__((optimize("O0"))) wait_sg(uint32_t desc_idx)
{
    int j=0;
    uint32_t status, errors;
    for (j = 0; j < CDMA_MAX_POLLING; j++) {
        status = dma_ctrl_read(CDMA_REG_SR);
        if (status & CDMA_REG_SR_ALL_ERR_MASK) {
            printk(KERN_ERR "PL_DMA] ERROR DMA - SR: %x", status);
            errors = status & CDMA_REG_SR_ALL_ERR_MASK;
            dma_ctrl_write(CDMA_REG_SR, errors & CDMA_REG_SR_ERR_RECOVER_MASK);
            dma_ctrl_write(CDMA_REG_CR, CDMA_REG_CR_RESET);
            return -EDMA_TX;
        }
        if ((dma_desc_read(desc_idx, CDMA_DESC_STATUS) & CDMA_REG_DESC_TX_COMPL) == CDMA_REG_DESC_TX_COMPL){
            dma_ctrl_write(CDMA_REG_SR, CDMA_REG_SR_IOCIRQ);
            return SUCCESS;
        }
    }
    printk(KERN_ERR "PL_DMA] DMA polling count reaches zero !! \n");    
    return -EDMA_TX;
}

int __attribute__((optimize("O0"))) wait_simple(void)
{
    int j=0;
    uint32_t status, errors;
    for (j = 0; j < CDMA_MAX_POLLING; j++) {
        status = dma_ctrl_read(CDMA_REG_SR);
        if (status & CDMA_REG_SR_ALL_ERR_MASK) {
            printk(KERN_ERR "PL_DMA] ERROR DMA - SR: %x", status);
            errors = status & CDMA_REG_SR_ALL_ERR_MASK;            
            dma_ctrl_write(CDMA_REG_SR, errors & CDMA_REG_SR_ERR_RECOVER_MASK);
            dma_ctrl_write(CDMA_REG_CR, CDMA_REG_CR_RESET); //DMA RESET
            return -EDMA_TX;
        }        
        if ((status & CDMA_REG_SR_TX_COMPL) == CDMA_REG_SR_TX_COMPL){
            dma_ctrl_write(CDMA_REG_SR, CDMA_REG_SR_IOCIRQ); //DMA RESET
            return SUCCESS;
        }
    }
    printk(KERN_ERR "PL_DMA] DMA polling count reaches zero !! \n");
    return -EDMA_TX;
}
#endif

#ifdef INTR_ENABLE
static struct completion trans_compl;

irqreturn_t pl_dma_irq_handler(int irq, void *data)
{ 
    struct cdma_dev_t *dev = (struct cdma_dev_t *)data;    
    uint32_t status, errors;
    PL_DMA_LOG("------------------------------------- \n");
    PL_DMA_LOG("Interrupt occured ! | SR: %x | CR: %x \n", dma_ctrl_read(CDMA_REG_SR), dma_ctrl_read(CDMA_REG_CR));
    status = dma_ctrl_read(CDMA_REG_SR);
    if (status & CDMA_REG_SR_ERR_IRQ) {
        printk("ERR IRQ - SR: %x, OPCODE: %x", status, dev->opcode);
        switch (status)
        if (status & CDMA_REG_SR_SG_DEC_ERR)
            printk(KERN_ERR "PL_DMA] DMA SG-Decoding error ");
        if (status & CDMA_REG_SR_SG_SLV_ERR)
            printk(KERN_ERR "PL_DMA] DMA SG-Slave error ");            
        if (status & CDMA_REG_SR_DMA_DEC_ERR)
            printk(KERN_ERR "PL_DMA] DMA Decoding error ");
        if (status & CDMA_REG_SR_DMA_SLAVE_ERR)
            printk(KERN_ERR "PL_DMA] DMA Slave error ");
        if (status & CDMA_REG_SR_DMA_INT_ERR)
            printk(KERN_ERR "PL_DMA] DMA Internal error ");       
        errors = status & CDMA_REG_SR_ALL_ERR_MASK;
        dma_ctrl_write(CDMA_REG_SR, errors & CDMA_REG_SR_ERR_RECOVER_MASK);
        dma_ctrl_write(CDMA_REG_CR, CDMA_REG_CR_RESET);
        dev->err = -EDMA_INTR;
        PL_DMA_LOG("------------------------------------- \n");        
        return IRQ_HANDLED;
    } 
    if (status & CDMA_REG_SR_DLY_CNT_IRQ) {
        /*
         * Device takes too long to do the transfer when user requires
         * responsiveness.
         */
        printk(KERN_ERR "PL_DMA] DMA Inter-packet latency too long\n");
        dev->err = -EDMA_INTR;
        dma_ctrl_write(CDMA_REG_SR, CDMA_REG_SR_IOCIRQ | CDMA_REG_SR_DLY_CNT_IRQ | CDMA_REG_SR_ERR_IRQ);
        PL_DMA_LOG("------------------------------------- \n");        
        return IRQ_HANDLED;        
    }
    PL_DMA_LOG("COMPLETE - SR: %x, OPCODE: %x", status, dev->opcode);
    dev->err = 0;
    dma_ctrl_write(CDMA_REG_SR, CDMA_REG_SR_IOCIRQ | CDMA_REG_SR_DLY_CNT_IRQ | CDMA_REG_SR_ERR_IRQ);
    PL_DMA_LOG("------------------------------------- \n");
    complete(&trans_compl);
    return IRQ_HANDLED;
}
#endif


int dma_single_tx(uint32_t src_low, uint32_t src_high, uint32_t dst_low, uint32_t dst_high, uint32_t copy_len)
{
    int ret=0;    
    PL_DMA_LOG("SRC_L: %x", src_low);
    PL_DMA_LOG("SRC_H: %x", src_high);
    PL_DMA_LOG("DST_L: %x", dst_low);
    PL_DMA_LOG("DST_H: %x", dst_high);
    dma_ctrl_write(CDMA_REG_SR, CDMA_REG_SR_IOCIRQ);
    PL_DMA_LOG("CR: %x \n", dma_ctrl_read(CDMA_REG_CR));
    PL_DMA_LOG("SR: %x \n", dma_ctrl_read(CDMA_REG_SR));
    dma_ctrl_write(CDMA_REG_SA_L, src_low);
    dma_ctrl_write(CDMA_REG_SA_H, src_high);
    dma_ctrl_write(CDMA_REG_DA_L, dst_low);
    dma_ctrl_write(CDMA_REG_DA_H, dst_high);
    dma_ctrl_write(CDMA_REG_BYTETRANS, copy_len);
#ifdef INTR_ENABLE
    dma_ctrl_write(CDMA_REG_CR, CDMA_REG_CR_ALL_IRQ_MASK | CDMA_REG_CR_DELAY_MASK | (0x1 << CDMA_COALESCE_SHIFT));
    init_completion(&trans_compl);
    ret = wait_for_completion_timeout(&trans_compl, msecs_to_jiffies(WAIT_INTR));
    if (ret == 0) {
        printk(KERN_ERR "PL_DMA] DMA Timeout !! \n");
        return -EDMA_TX;
    }
    if (cdma_dev->err < 0) {
        printk("PL_DMA] DMA Error !! \n");
        return -EDMA_INTR;
    }
#else
    ret = wait_simple();
    if (ret < 0)
        return -EDMA_TX;
#endif
    return SUCCESS;
}

int dma_sg_tx(uint32_t desc_idx, uint32_t next_desc)
{
    int ret=0;
#ifdef INTR_ENABLE    
    if (desc_idx < CDMA_MAX_COALESCE) {
        int num_intr = desc_idx << CDMA_COALESCE_SHIFT;
        dma_ctrl_write(CDMA_REG_CR, CDMA_REG_CR_SG_EN | CDMA_REG_CR_ALL_IRQ_MASK | CDMA_REG_CR_DELAY_MASK | num_intr);
        dma_ctrl_write(CDMA_REG_SR, CDMA_REG_SR_IOCIRQ);
        PL_DMA_LOG("COALESCE: %x (%d) \n", num_intr, desc_idx);
        PL_DMA_LOG("1) :DESC_ADDR: (%llx - %x) \n", cdma_dev->desc_mem_base, next_desc-0x40);
        PL_DMA_LOG("CR: %x \n", dma_ctrl_read(CDMA_REG_CR));
        PL_DMA_LOG("SR: %x \n", dma_ctrl_read(CDMA_REG_SR));
        dma_ctrl_write(CDMA_CURDESC_PNTR_L, cdma_dev->desc_mem_base);
        dma_ctrl_write(CDMA_CURDESC_PNTR_H, HIGH_ADDR);
        wmb();
        dma_ctrl_write(CDMA_TAILDESC_PNTR_L, next_desc-0x40);
        dma_ctrl_write(CDMA_TAILDESC_PNTR_H, HIGH_ADDR);
        init_completion(&trans_compl);
        ret = wait_for_completion_timeout(&trans_compl, msecs_to_jiffies(WAIT_INTR));
        if (ret == 0) {
            printk("PL_DMA] DMA Timeout !! \n");
            return -EDMA_TX;
        }
    } else {
        int chunk_desc, remain_desc, k, offset;
        uint32_t start_desc, end_desc;
        chunk_desc = (desc_idx / CDMA_MAX_COALESCE);
        remain_desc = (desc_idx % CDMA_MAX_COALESCE);
        for (k = 0; k < chunk_desc; k++) {
            int num_intr = CDMA_MAX_COALESCE << CDMA_COALESCE_SHIFT;
            offset = k * CDMA_MAX_COALESCE * 0x40;
            start_desc = cdma_dev->desc_mem_base + offset;
            end_desc = start_desc + ((CDMA_MAX_COALESCE - 1) * 0x40);
            PL_DMA_LOG("%d - %d) DESC_ADDR: (%x - %x) \n", k, chunk_desc, start_desc, end_desc);
            dma_ctrl_write(CDMA_REG_CR, CDMA_REG_CR_SG_EN | CDMA_REG_CR_ALL_IRQ_MASK | CDMA_REG_CR_DELAY_MASK | num_intr);
            dma_ctrl_write(CDMA_REG_SR, CDMA_REG_SR_IOCIRQ);
            PL_DMA_LOG("CR: %x \n", dma_ctrl_read(CDMA_REG_CR));
            PL_DMA_LOG("SR: %x \n", dma_ctrl_read(CDMA_REG_SR));
            dma_ctrl_write(CDMA_CURDESC_PNTR_L, start_desc);
            dma_ctrl_write(CDMA_CURDESC_PNTR_H, HIGH_ADDR);
            wmb();
            dma_ctrl_write(CDMA_TAILDESC_PNTR_L, end_desc);
            dma_ctrl_write(CDMA_TAILDESC_PNTR_H, HIGH_ADDR);            
            init_completion(&trans_compl);
            ret = wait_for_completion_timeout(&trans_compl, msecs_to_jiffies(WAIT_INTR));
            if (ret == 0) {
                printk(KERN_ERR "PL_DMA] DMA Timeout !! (%d - %d)\n", k, chunk_desc);
                return -EDMA_TX;
            }
            if (cdma_dev->err < 0) {
                printk(KERN_ERR "PL_DMA] DMA Error !! (%d - %d)\n", k, chunk_desc);
                return -EDMA_INTR;
            }
        }
        if (remain_desc) {
            int num_intr = remain_desc << CDMA_COALESCE_SHIFT;
            start_desc = end_desc + 0x40;
            end_desc = next_desc-0x40;
            PL_DMA_LOG("%d) DESC_ADDR: (%x - %x) \n", remain_desc, start_desc, end_desc);
            dma_ctrl_write(CDMA_REG_CR, CDMA_REG_CR_SG_EN | CDMA_REG_CR_ALL_IRQ_MASK | CDMA_REG_CR_DELAY_MASK | num_intr);
            dma_ctrl_write(CDMA_REG_SR, CDMA_REG_SR_IOCIRQ);
            PL_DMA_LOG("CR: %x \n", dma_ctrl_read(CDMA_REG_CR));
            PL_DMA_LOG("SR: %x \n", dma_ctrl_read(CDMA_REG_SR));
            dma_ctrl_write(CDMA_CURDESC_PNTR_L, start_desc);
            dma_ctrl_write(CDMA_CURDESC_PNTR_H, HIGH_ADDR);
            wmb();
            dma_ctrl_write(CDMA_TAILDESC_PNTR_L, end_desc);
            dma_ctrl_write(CDMA_TAILDESC_PNTR_H, HIGH_ADDR);            
            init_completion(&trans_compl);
            ret = wait_for_completion_timeout(&trans_compl, msecs_to_jiffies(WAIT_INTR));
            if (ret == 0) {
                printk(KERN_ERR "PL_DMA] DMA Timeout !! (%d - %d)\n", k, remain_desc);
                return -EDMA_TX;
            }
            if (cdma_dev->err < 0) {
                printk(KERN_ERR "PL_DMA] DMA Error !! (%d - %d)\n", k, chunk_desc);
                return -EDMA_INTR;
            }
        }
    }
    return ret;
#else
    dma_ctrl_write(CDMA_REG_CR, CDMA_REG_CR_SG_EN);
    dma_ctrl_write(CDMA_REG_SR, CDMA_REG_SR_IOCIRQ);
    dma_ctrl_write(CDMA_CURDESC_PNTR_L, cdma_dev->desc_mem_base);
    dma_ctrl_write(CDMA_CURDESC_PNTR_H, HIGH_ADDR);
    wmb();
    dma_ctrl_write(CDMA_TAILDESC_PNTR_L, next_desc-0x40);
    dma_ctrl_write(CDMA_TAILDESC_PNTR_H, HIGH_ADDR);
    ret=wait_simple();
    return ret;
#endif    
}
/* For file operations */
long pl_dma_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    ioctl_info user_info;
    void *__user arg_ptr;
    uint32_t opcode_type;

    /* 
     * In x86 platform, FPGA DRAM is connected via PCIe, so it has a 32-bit address (1GB) 
     * In ARM platform, FPGA DRAM is 64-bit address (4GB)
     */
#ifdef __x86_64__    
    uint32_t src_pa, dst_pa;
#elif __aarch64__    
    uint64_t src_pa, dst_pa;
#endif

    void *dmabuf;
    dma_addr_t dma_handle;
#ifdef __x86_64__
    //uint32_t addr_hi;
    //uint32_t addr_lo;
#endif

    arg_ptr = (void __user *) arg;
    if (copy_from_user(&user_info, arg_ptr, sizeof(user_info))) {
        return -EFAULT;
    }
    dma_ctrl_write(CDMA_REG_CR,  CDMA_REG_CR_RESET);

    switch (cmd)
    {
        case MATMUL:
        case MATMUL_DECOUPLED:
        {
            PERF_START(start_dsc_init);
            if (cmd == MATMUL_DECOUPLED) {
                opcode_type = OPCODE_MAT_MUL_DECOUPLED_32x1;
                matmul_decoupled(&user_info, opcode_type);
            }
            else {
                opcode_type = OPCODE_MAT_MUL;
                matmul(&user_info, opcode_type);
            }
            PERF_END(start_dsc_init, end_dsc_init, desc_time);

            PERF_START(start_dma_tx);
            if (dma_sg_tx(user_info.desc_idx, user_info.next_desc) < 0)
                return -EDMA_TX;
            PERF_END(start_dma_tx, end_dma_tx, vm_time);
            break;
        }
        case LUT_OPS:
        {
            opcode_type = OPCODE_LUT;

            PERF_START(start_dsc_init);
            lut_op(&user_info, opcode_type);
            PERF_END(start_dsc_init, end_dsc_init, desc_time);

            PERF_START(start_dma_tx);
            if (dma_sg_tx(user_info.desc_idx, user_info.next_desc) < 0)
                return -EDMA_TX;
            PERF_END(start_dma_tx, end_dma_tx, lut_time);
            break;
        }      
        case GEMM:
        {
            PERF_START(start_dsc_init);
            opcode_type = OPCODE_MAT_MUL;
            gemm(&user_info, opcode_type);
            PERF_END(start_dsc_init, end_dsc_init, desc_time);

            PERF_START(start_dma_tx);
            if (dma_sg_tx(user_info.desc_idx, user_info.next_desc) < 0)
                return -EDMA_TX;
            PERF_END(start_dma_tx, end_dma_tx, gemm_time);
            break;
        }
        case GEMM_BIAS:
        {
            PERF_START(start_dsc_init);
            opcode_type = OPCODE_MAT_MUL;
            gemm_bias(&user_info, opcode_type);
            PERF_END(start_dsc_init, end_dsc_init, desc_time);

            PERF_START(start_dma_tx);
            if (dma_sg_tx(user_info.desc_idx, user_info.next_desc) < 0)
                return -EDMA_TX;
            PERF_END(start_dma_tx, end_dma_tx, gemm_bias_time);
            break;
        }
        case ELEWISE_ADD:
        case ELEWISE_SUB:
        case ELEWISE_MUL:
        {
            if (cmd == ELEWISE_ADD)
                opcode_type = OPCODE_ELE_ADD;
            else if (cmd == ELEWISE_SUB)
                opcode_type = OPCODE_ELE_SUB;
            else if (cmd == ELEWISE_MUL)
                opcode_type = OPCODE_ELE_MUL;

            PERF_START(start_dsc_init);
            elewise(&user_info, opcode_type);
            PERF_END(start_dsc_init, end_dsc_init, desc_time);

            PERF_START(start_dma_tx);
            if (dma_sg_tx(user_info.desc_idx, user_info.next_desc) < 0)
                return -EDMA_TX;
            PERF_END(start_dma_tx, end_dma_tx, elew_time);
            break;
        }
        case ACTIVE_SIGMOID:
        case ACTIVE_TANH:
        {
            if(cmd==ACTIVE_SIGMOID)
                opcode_type = OPCODE_ACT_SIG;
            else if(cmd==ACTIVE_TANH)
                opcode_type = OPCODE_ACT_TAN;

            PERF_START(start_dsc_init);
            activation(&user_info, opcode_type);
            PERF_END(start_dsc_init, end_dsc_init, desc_time);

            PERF_START(start_dma_tx);
            if (dma_sg_tx(user_info.desc_idx, user_info.next_desc) < 0)
                return -EDMA_TX;
            PERF_END(start_dma_tx, end_dma_tx, active_time);
            break;
        }
        case BIAS_ADD:
        case BIAS_SUB:
        case BIAS_MUL:
        {
            if(cmd==BIAS_ADD)
                opcode_type = OPCODE_ELE_ADD;
            else if(cmd==BIAS_SUB)
                opcode_type = OPCODE_ELE_SUB;
            else if(cmd==BIAS_MUL)
                opcode_type = OPCODE_ELE_MUL;

            bias_op(&user_info, opcode_type);
            PERF_START(start_dma_tx);
            if (dma_sg_tx(user_info.desc_idx, user_info.next_desc) < 0)
                return -EDMA_TX;
            PERF_END(start_dma_tx, end_dma_tx, elew_time);
            break;
        }
        /* Memory copy using DMA engine */
        case MEMCPY_PL2PL:
        {
            PL_DMA_LOG("Memory copy PL -> PL");
            src_pa = va_to_pa(user_info.srcA_va);
            dst_pa = va_to_pa(user_info.dstC_va);
            PL_DMA_LOG("src addr: %llx -> %llx ", user_info.srcA_va, src_pa);
            PL_DMA_LOG("dst addr: %llx -> %llx ", user_info.dstC_va, dst_pa);
            PERF_START(start_dma_tx);
            if (dma_single_tx(src_pa, HIGH_ADDR, dst_pa, HIGH_ADDR, user_info.srcA_size) < 0)
                return -EDMA_TX;
            PERF_END(start_dma_tx, end_dma_tx, memcpy_time);
            break;
        }        
        case MEMCPY_PL2PS:
        {
            uint32_t copy_len, num_chunk, i;
            PL_DMA_LOG("Memory copy PL -> PS");
            copy_len = (user_info.srcA_size > CPY_CHUNK_SIZE) ? CPY_CHUNK_SIZE : \
                                                      user_info.srcA_size;
            num_chunk = (user_info.srcA_size % CPY_CHUNK_SIZE) ? (user_info.srcA_size / CPY_CHUNK_SIZE) + 1 : \
                                                      (user_info.srcA_size / CPY_CHUNK_SIZE);
#ifdef __x86_64__
            dmabuf=dma_alloc_coherent(NULL, copy_len, &dma_handle, GFP_KERNEL);
#elif defined __aarch64__
            /* In the ARM platform, PS DMA buffer is used as dmabuf */
            dmabuf = ps_dma_t->kern_addr;
            dma_handle = ps_dma_t->bus_addr;
#endif
            if (dmabuf == NULL) {
                printk(KERN_ERR "DMA buffer allocation failed");
                return -EFAULT;
            }
            PL_DMA_LOG("copy_len: %d \n", copy_len);
            PL_DMA_LOG("num_chunk: %d \n", num_chunk);
            for (i = 0; i < num_chunk; i++) {
                src_pa = va_to_pa(user_info.srcA_va + (i * CPY_CHUNK_SIZE)) - pa_offset;
                dst_pa = dma_handle;

                PERF_START(start_dma_tx);
#ifdef __x86_64__                
                /* pci control register not operated */
                //addr_hi = dst_pa >> 0x20;
                //addr_lo = dst_pa & 0xFFFFFFFF;
                //pci_ctrl_write(AXIBAR2PCIEBAR_0U, addr_hi);
                //pci_ctrl_write(AXIBAR2PCIEBAR_0L, addr_lo);
                dst_pa = dst_pa & AXI_MASK;
                dst_pa = dst_pa + AXIBAR;
                /* In x86 platform, high address of dst_pa is always 0 because dst_pa is 32 bits */
                if (dma_single_tx(src_pa, HIGH_ADDR, dst_pa, HIGH_ADDR, copy_len) < 0)
                    return -EDMA_TX;                
#elif defined __aarch64__
                if (dma_single_tx(src_pa, HIGH_ADDR, dst_pa, dst_pa>>0x20, copy_len) < 0)
                    return -EDMA_TX;
#endif
                PERF_END(start_dma_tx, end_dma_tx, memcpy_time);
                if (copy_to_user(user_info.dstC_ptr + (i * CPY_CHUNK_SIZE), dmabuf, copy_len)) {
                    return -EFAULT;
                }
            }
#ifdef __x86_64__
            dma_free_coherent(NULL, user_info.srcA_size, dmabuf, dma_handle);
#endif
            break;
        }
        case MEMCPY_PS2PL:
        {
            uint32_t copy_len, num_chunk, i;
            PL_DMA_LOG("Memory copy PS -> PL");
            copy_len = (user_info.srcA_size > CPY_CHUNK_SIZE) ? CPY_CHUNK_SIZE : \
                                                      user_info.srcA_size;
            num_chunk = (user_info.srcA_size % CPY_CHUNK_SIZE) ? (user_info.srcA_size / CPY_CHUNK_SIZE) + 1 : \
                                                      (user_info.srcA_size / CPY_CHUNK_SIZE);

#ifdef __x86_64__
            dmabuf=dma_alloc_coherent(NULL, copy_len, &dma_handle, GFP_KERNEL);
#elif defined __aarch64__
            /* In the ARM platform, PS DMA buffer is used as dmabuf */
            dmabuf = ps_dma_t->kern_addr;
            dma_handle = ps_dma_t->bus_addr;
#endif
            if (dmabuf == NULL) {
                printk(KERN_ERR "DMA buffer allocation failed");
                return -EFAULT;
            }
            PL_DMA_LOG("copy_len: %d \n", copy_len);
            PL_DMA_LOG("num_chunk: %d \n", num_chunk);

            for (i = 0; i < num_chunk; i++) {
                if (copy_from_user(dmabuf, user_info.srcA_ptr + (i * CPY_CHUNK_SIZE), copy_len)) {
                   return -EFAULT;         
                }
                src_pa = dma_handle;
                dst_pa = va_to_pa(user_info.dstC_va) - pa_offset;
#ifdef __x86_64__
                /* pci control register not operated */
                //addr_hi = src_pa >> 0x20;
                //addr_lo = src_pa & 0xFFFFFFFF;
                //pci_ctrl_write(AXIBAR2PCIEBAR_0U, addr_hi);
                //pci_ctrl_write(AXIBAR2PCIEBAR_0L, addr_lo);
                src_pa = src_pa & AXI_MASK;
                src_pa = src_pa + AXIBAR;
                /* In x86 platform, high address of src_pa is always 0 because src_pa is 32 bits */
                if (dma_single_tx(src_pa, HIGH_ADDR, dst_pa, HIGH_ADDR, copy_len) < 0)
                    return -EDMA_TX;
#elif defined __aarch64__
                if (dma_single_tx(src_pa, src_pa>>0x20, dst_pa, HIGH_ADDR, copy_len) < 0)
                    return -EDMA_TX;
#endif
            }
#ifdef __x86_64__
            dma_free_coherent(NULL, user_info.srcA_size, dmabuf, dma_handle);
#endif
            break;
        }
        case TIME_INIT:
        {
            vm_time = 0;
            ew_add_time = 0;
            ew_mul_time = 0;
            ew_sub_time = 0;
            desc_time = 0;
            break;
        }
        case GET_VM_TIME:
        {
            return (long long unsigned int)(vm_time/1000);
            break;
        }
        case GET_EWAdd_TIME:
        {
            return (long long unsigned int)(ew_add_time/1000);
            break;
        }
        case GET_EWMul_TIME:
        {
            return (long long unsigned int)(ew_mul_time/1000);
            break;
        }
        case GET_EWSub_TIME:
        {
            return (long long unsigned int)(ew_sub_time/1000);
            break;
        }
        case GET_DESC_TIME:
        {
            return (long long unsigned int)(desc_time/1000);
            break;
        }    
        default :
            printk(KERN_ERR "Invalid IOCTL:%d\n", cmd);
            break;
    }
    return SUCCESS;
}

int pl_dma_open(struct inode *inode, struct file *file)
{
    int ret;
    ret = mutex_trylock(&dma_lock);
    if (ret == 0) {
        printk(KERN_ERR "DMA is in use\n");
        return -EFAULT;
    }    
    return SUCCESS;
}

int pl_dma_release(struct inode *inode, struct file *file)
{
    dma_ctrl_write(CDMA_REG_CR,  CDMA_REG_CR_RESET); //DMA RESET
    wmb();
    mutex_unlock(&dma_lock);
    return SUCCESS;
}

int pl_dma_mmap(struct file *filp, struct vm_area_struct *vma)
{
    return SUCCESS;
}

ssize_t pl_dma_read(struct file *f, char *buf, size_t nbytes, loff_t *ppos)
{
    return SUCCESS;
}

MODULE_DESCRIPTION("PL-DMA file operations");
MODULE_AUTHOR("KU-Leewj");
MODULE_LICENSE("GPL");
