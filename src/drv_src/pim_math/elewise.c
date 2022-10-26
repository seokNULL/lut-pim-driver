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

#include "../../../include/pim.h"
#include "../dma_lib_kernel.h"



int elewise(ioctl_info *user_info, uint32_t opcode)
{
    int ret;
	uint32_t desc_idx, next_desc, p_loop_var, r_loop_var, p, r;
	uint32_t srcA_size, srcB_size, dstC_size, p_size, q_size, r_size;
    uint32_t A_pa, B_pa, C_pa, r_size_aligned, offset;
	uint64_t srcA_va, srcB_va, bias_va, dstC_va, A_va, B_va, C_va;
	bool r_need_align;

    PL_DMA_LOG("Element-wise (OPCODE: %d)", opcode);
	ret = 0;
	srcA_va = user_info->srcA_va;
    srcB_va = user_info->srcB_va;
    bias_va = user_info->bias_va;
    dstC_va = user_info->dstC_va;

    srcA_size = user_info->srcA_size;
    srcB_size = user_info->srcB_size;
    dstC_size = user_info->dstC_size;
    p_size = user_info->p_size;
    q_size = user_info->q_size;
    r_size = user_info->r_size;

	desc_idx = 0;
	next_desc = cdma_dev->desc_mem_base + 0x40;

    p_loop_var = p_size;
    r_loop_var = r_size;
    r_need_align = ( (r_size & 0x1F)) ? 1 : 0;

    if (r_need_align)
        r_size_aligned = (r_size / 32 + 1) * 32;
    else
        r_size_aligned = r_size;

    for(p = 0; p < p_loop_var; p++){
        for (r = 0; r < r_loop_var; r = r + (vACC_SIZE * NUM_BANKS)){
            dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
            dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
            dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
        }
    }
    dma_desc_write(desc_idx, CDMA_DESC_STATUS, 0);
    desc_idx = 0;

    for (p = 0; p < p_loop_var; p++){
        for(r = 0; r < r_loop_var; r = r + (vACC_SIZE * NUM_BANKS)){
            offset = (p * r_size_aligned * TYPE_SIZE) + (r * TYPE_SIZE);
            A_va = srcA_va + offset;
            B_va = srcB_va + offset;
            C_va = dstC_va + offset;

            A_pa = va_to_pa(A_va) - pa_offset;
            B_pa = va_to_pa(B_va) - pa_offset;
            C_pa = va_to_pa(C_va) - pa_offset;
            
            PL_DMA_LOG("A_PA: %x\r\n", A_pa);
            PL_DMA_LOG("B_PA: %x\r\n", B_pa);
            PL_DMA_LOG("C_PA: %x\r\n", C_pa);


            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, A_pa);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, PL_DUMMY_PA);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, 0x0);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, vACC_SIZE * NUM_BANKS * TYPE_SIZE);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x2 | (opcode <<4));
            desc_idx++;
            next_desc+=0x40;

            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, B_pa);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, PL_DUMMY_PA);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, 0x0);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, vACC_SIZE * NUM_BANKS * TYPE_SIZE);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x4 | (opcode <<4));
            desc_idx++;
            next_desc+=0x40;

            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, PL_DUMMY_PA);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, 0);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, C_pa);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, vACC_SIZE * NUM_BANKS * TYPE_SIZE);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x9 | (opcode <<4));
            desc_idx++;
            next_desc+=0x40;
        }
    }

    dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
    dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
    dma_desc_write(desc_idx, CDMA_DESC_SA_L, PL_DUMMY_PA);
    dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
    dma_desc_write(desc_idx, CDMA_DESC_DA_L, cdma_dev->dram_base + CONF_OFFSET_HPC_CLR);
    dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
    dma_desc_write(desc_idx, CDMA_DESC_LEN, 0x40);
    dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x0 | (opcode <<4));

    pim_conf_write(CONF_OFFSET_HPC_CLR, 0x0);
    pim_conf_write(CONF_OFFSET_AIM_WORKING, 0x1);
    wmb();
    pim_conf_write(CONF_OFFSET_DESC_BASE, cdma_dev->desc_mem_base);
    pim_conf_write(CONF_OFFSET_DESC_NUM, desc_idx * 0x40);

	user_info->desc_idx = desc_idx;
	user_info->next_desc = next_desc;
    PL_DMA_LOG("DESC WR DONE[%llx - %x] idx: %d (size: %d, %d, %d)\n",cdma_dev->desc_mem_base, next_desc-0x40, user_info->desc_idx, p_size, q_size, r_size);
    return ret;	
}

int activation(ioctl_info *user_info, uint32_t opcode)
{
    int ret;
	uint32_t desc_idx, next_desc, p_loop_var, r_loop_var, p, r;
	uint32_t srcA_size, srcB_size, dstC_size, p_size, q_size, r_size;
    uint32_t A_pa, B_pa, C_pa, offset;
	uint64_t srcA_va, srcB_va, bias_va, dstC_va, A_va, B_va, C_va;

    PL_DMA_LOG("Activation (OPCODE: %d)", opcode);
	ret = 0;
	srcA_va = user_info->srcA_va;
    srcB_va = user_info->srcB_va;
    bias_va = user_info->bias_va;
    dstC_va = user_info->dstC_va;

    srcA_size = user_info->srcA_size;
    srcB_size = user_info->srcB_size;
    dstC_size = user_info->dstC_size;
    p_size = user_info->p_size;
    q_size = user_info->q_size;
    r_size = user_info->r_size;

	desc_idx = 0;
	next_desc = cdma_dev->desc_mem_base + 0x40;

    p_loop_var = p_size;
    r_loop_var = r_size;

    dma_desc_write(desc_idx, CDMA_DESC_STATUS, 0);
    for(p = 0; p < p_loop_var; p++){
        for(r = 0; r < r_loop_var; r = r + (vACC_SIZE * NUM_BANKS)){
            dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
            dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
        }
    }
    dma_desc_write(desc_idx, CDMA_DESC_STATUS, 0);
    desc_idx = 0;

    A_va = srcA_va;
    A_pa = va_to_pa(A_va) - pa_offset;

    dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
    dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
    dma_desc_write(desc_idx, CDMA_DESC_SA_L, A_pa);
    dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
    dma_desc_write(desc_idx, CDMA_DESC_DA_L, PL_DUMMY_PA);
    dma_desc_write(desc_idx, CDMA_DESC_DA_H, 0x0);
    dma_desc_write(desc_idx, CDMA_DESC_LEN, 0x40);
    dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x2 | (opcode <<4));
    desc_idx++;
    next_desc+=0x40;

    for(p = 0; p < p_loop_var; p++){
        for(r = 0; r < r_loop_var; r = r + (vACC_SIZE * NUM_BANKS)){  
            offset = (p * r_size * TYPE_SIZE) + (r * TYPE_SIZE);
            B_va = srcB_va + offset;
            C_va = dstC_va + offset;

            B_pa = va_to_pa(B_va) - pa_offset;
            C_pa = va_to_pa(C_va) - pa_offset;

            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, B_pa);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, PL_DUMMY_PA);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, 0x0);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, vACC_SIZE * NUM_BANKS * TYPE_SIZE);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x4 | (opcode <<4));
            desc_idx++;
            next_desc+=0x40;

            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, PL_DUMMY_PA);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, 0);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, C_pa);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, vACC_SIZE * NUM_BANKS * TYPE_SIZE);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x9 | (opcode <<4));
            desc_idx++;
            next_desc+=0x40;
        }
    }

    dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
    dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
    dma_desc_write(desc_idx, CDMA_DESC_SA_L, PL_DUMMY_PA);
    dma_desc_write(desc_idx, CDMA_DESC_SA_H, 0);
    dma_desc_write(desc_idx, CDMA_DESC_DA_L, cdma_dev->dram_base + CONF_OFFSET_HPC_CLR);
    dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
    dma_desc_write(desc_idx, CDMA_DESC_LEN, 0x40);
    dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x0 | (opcode <<4));

    pim_conf_write(CONF_OFFSET_HPC_CLR, 0x0);
    pim_conf_write(CONF_OFFSET_AIM_WORKING, 0x1);
    wmb();
    pim_conf_write(CONF_OFFSET_DESC_BASE, cdma_dev->desc_mem_base);
    pim_conf_write(CONF_OFFSET_DESC_NUM, desc_idx * 0x40);

	user_info->desc_idx = desc_idx;
	user_info->next_desc = next_desc;
    PL_DMA_LOG("DESC WR DONE[%llx - %x] idx: %d (size: %d, %d, %d)\n",cdma_dev->desc_mem_base, next_desc-0x40, user_info->desc_idx, p_size, q_size, r_size);
    return ret;	
}

int bias_op(ioctl_info *user_info, uint32_t opcode)
{
    int ret;
    uint32_t desc_idx, next_desc, r_loop_var, q_loop_var, q, r;
    uint32_t srcA_size, srcB_size, dstC_size, p_size, q_size, r_size;
    uint32_t A_pa, B_pa, C_pa, r_size_aligned, offset;
    uint64_t srcA_va, srcB_va, bias_va, dstC_va, A_va, B_va, C_va;
    bool r_need_align;

    PL_DMA_LOG("BISA_OP (OPCODE: %d)", opcode);
    ret = 0;
    srcA_va = user_info->srcA_va;
    srcB_va = user_info->srcB_va;
    bias_va = user_info->bias_va;
    dstC_va = user_info->dstC_va;

    srcA_size = user_info->srcA_size;
    srcB_size = user_info->srcB_size;
    dstC_size = user_info->dstC_size;
    p_size = user_info->p_size;
    q_size = user_info->q_size;
    r_size = user_info->r_size;

    desc_idx = 0;
    next_desc = cdma_dev->desc_mem_base + 0x40;

    q_loop_var = q_size;
    r_loop_var = r_size;
    r_need_align = ((r_size & 0x1F)) ? 1 : 0;

    if (r_need_align)
        r_size_aligned = (r_size / 32 + 1) * 32;
    else
        r_size_aligned = r_size;

    for (r = 0; r < r_loop_var; r = r + (vACC_SIZE * NUM_BANKS)){
        for (q = 0; q < q_loop_var; q++){
            //if (q == 0){
                dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
            //}
            dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
            dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
        }
    }
    dma_desc_write(desc_idx, CDMA_DESC_STATUS, 0);
    desc_idx = 0;

    for (r = 0; r < r_loop_var; r = r + (vACC_SIZE * NUM_BANKS)){
        for (q = 0; q < q_loop_var; q++){
            //if (q == 0){
                A_va = srcA_va + (r * TYPE_SIZE);
                A_pa = va_to_pa(A_va);
                dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
                dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
                dma_desc_write(desc_idx, CDMA_DESC_SA_L, A_pa);
                dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
                dma_desc_write(desc_idx, CDMA_DESC_DA_L, PL_DUMMY_PA);
                dma_desc_write(desc_idx, CDMA_DESC_DA_H, 0x0);
                dma_desc_write(desc_idx, CDMA_DESC_LEN, vACC_SIZE * NUM_BANKS * TYPE_SIZE);
                dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x2 | (opcode <<4));
                desc_idx++;
                next_desc+=0x40;
            //}
            offset = (q * r_size_aligned * TYPE_SIZE) + (r * TYPE_SIZE);
            B_va = srcB_va + offset;
            C_va = dstC_va + offset;            
            B_pa = va_to_pa(B_va);
            C_pa = va_to_pa(C_va);
    
            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, B_pa);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, PL_DUMMY_PA);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, 0x0);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, vACC_SIZE * NUM_BANKS * TYPE_SIZE);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x4 | (opcode <<4));
            desc_idx++;
            next_desc+=0x40;                    

            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, PL_DUMMY_PA);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, 0);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, C_pa);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, vACC_SIZE * NUM_BANKS * TYPE_SIZE);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x9 | (opcode <<4));
            desc_idx++;
            next_desc+=0x40;
        }
    }

    dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
    dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
    dma_desc_write(desc_idx, CDMA_DESC_SA_L, PL_DUMMY_PA);
    dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
    dma_desc_write(desc_idx, CDMA_DESC_DA_L, cdma_dev->dram_base + CONF_OFFSET_HPC_CLR);
    dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
    dma_desc_write(desc_idx, CDMA_DESC_LEN, 0x40);
    dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x0 | (opcode <<4));

    pim_conf_write(CONF_OFFSET_HPC_CLR, 0x0);
    pim_conf_write(CONF_OFFSET_AIM_WORKING, 0x1);
    wmb();
    pim_conf_write(CONF_OFFSET_DESC_BASE, cdma_dev->desc_mem_base);
    pim_conf_write(CONF_OFFSET_DESC_NUM, desc_idx * 0x40);

	user_info->desc_idx = desc_idx;
	user_info->next_desc = next_desc;
    PL_DMA_LOG("DESC WR DONE[%llx - %x] idx: %d (size: %d, %d, %d)\n",cdma_dev->desc_mem_base, next_desc-0x40, user_info->desc_idx, p_size, q_size, r_size);
    return ret;	
}