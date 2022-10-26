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




int matmul(ioctl_info *user_info, uint32_t opcode)
{
    int ret;
    uint32_t desc_idx, next_desc, r_loop_var, p, q, r;
    uint32_t srcA_size, srcB_size, dstC_size, p_size, q_size, r_size;
    uint64_t A_pa, B_pa, C_pa;
    uint64_t srcA_va, srcB_va, bias_va, dstC_va, A_va, B_va, C_va;

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
    r_loop_var = (r_size * sizeof(Bfloat16)) / 1024;

    desc_idx = 0;
    next_desc = cdma_dev->desc_mem_base + 0x40;

    for (p = 0; p < p_size; p++) {
        for(r = 0; r < r_loop_var; r++) {
            for(q = 0; q < q_size; q = q + 32) {
                dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
                dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
            }
            dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
            dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
            dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
        }
    }
    dma_desc_write(desc_idx, CDMA_DESC_STATUS, 0);
    desc_idx = 0;

    for (p = 0; p < p_size; p++) {
        for(r=0;r<r_loop_var;r++){
            for(q=0;q<q_size;q=q+32){
                //read A
                A_va = srcA_va + (q * sizeof(Bfloat16)) + ((q_size * sizeof(Bfloat16)) * p);
                A_pa = va_to_pa(A_va) - pa_offset;

                dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
                dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
                dma_desc_write(desc_idx, CDMA_DESC_SA_L, A_pa);
                dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
                dma_desc_write(desc_idx, CDMA_DESC_DA_L, PL_DUMMY_PA);
                dma_desc_write(desc_idx, CDMA_DESC_DA_H, 0x0);
                dma_desc_write(desc_idx, CDMA_DESC_LEN, 64);
                dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x2 | (opcode <<4));
                desc_idx++;
                next_desc+=0x40;
                
                //read B
                B_va = srcB_va + (q_size * 1024 * r) + (q * 1024);
                B_pa = va_to_pa(B_va) - pa_offset;
                // PL_DMA_LOG("%5d %5d %5d] A: %llx --> %llx \n", p, r, q, A_va, A_pa);
                // PL_DMA_LOG("%5d %5d %5d] B: %llx --> %llx \n", p, r, q, B_va, B_pa);

                dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
                dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
                dma_desc_write(desc_idx, CDMA_DESC_SA_L, B_pa);
                dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
                dma_desc_write(desc_idx, CDMA_DESC_DA_L, PL_DUMMY_PA);
                dma_desc_write(desc_idx, CDMA_DESC_DA_H, 0x0);
                dma_desc_write(desc_idx, CDMA_DESC_LEN, 32*1024);
                dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x4 | (opcode <<4));
                desc_idx++;
                next_desc+=0x40;
            }
            //write C
            C_va = dstC_va + (r * 1024) + (p * r_size * sizeof(Bfloat16));
            C_pa = va_to_pa(C_va) - pa_offset;
            // PL_DMA_LOG("%5d %5d %5d] C: %llx --> %llx \n", p, r, q, C_va, C_pa);

            //dummy write barrier
            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, cdma_dev->conf_mem_base);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, cdma_dev->conf_mem_base+1024);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, 1024);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x0 | (opcode <<4));
            desc_idx++;
            next_desc+=0x40;


            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, cdma_dev->conf_mem_base);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, C_pa);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, 1024);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x9 | (opcode <<4));
            desc_idx++;
            next_desc+=0x40;

            //dummy write barrier
            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, cdma_dev->conf_mem_base);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, cdma_dev->conf_mem_base+1024);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, 1024);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x0 | (opcode <<4));
            desc_idx++;
            next_desc+=0x40;

        }
    }

    dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
    dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
    dma_desc_write(desc_idx, CDMA_DESC_SA_L, PL_DUMMY_PA);
    dma_desc_write(desc_idx, CDMA_DESC_SA_H, 0x0);
    dma_desc_write(desc_idx, CDMA_DESC_DA_L, cdma_dev->dram_base + CONF_OFFSET_HPC_CLR);
    dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
    dma_desc_write(desc_idx, CDMA_DESC_LEN, 64);
    dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x0 | (opcode <<4));

    pim_conf_write(CONF_OFFSET_HPC_CLR, 0x0);
    pim_conf_write(CONF_OFFSET_AIM_WORKING, 0x1);
    wmb();
    pim_conf_write(CONF_OFFSET_DESC_BASE, cdma_dev->desc_mem_base);
    pim_conf_write(CONF_OFFSET_DESC_NUM, desc_idx * 64);

    user_info->desc_idx = desc_idx;
    user_info->next_desc = next_desc;
    // PL_DMA_LOG("DESC WR DONE[%llx - %x] idx: %d (size: %d, %d, %d)\n",cdma_dev->desc_mem_base, next_desc-0x40, user_info->desc_idx, p_size, q_size, r_size);
    return ret;
}


int gemm(ioctl_info *user_info, uint32_t opcode)
{
    int ret;
    uint32_t desc_idx, next_desc, p_loop_var, r_loop_var, q_loop_var, p, q, r;
    uint32_t srcA_size, srcB_size, dstC_size, p_size, q_size, r_size;     
    uint32_t A_pa, B_pa, C_pa;
    uint64_t srcA_va, srcB_va, bias_va, dstC_va, A_va, B_va, C_va;

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
    r_loop_var = (r_size * sizeof(Bfloat16)) / 1024;
    q_loop_var = q_size;

    //Matrix-Multiplication
    for (p = 0; p < p_loop_var; p++) {
        for (r = 0; r < r_loop_var; r++) {
            for (q = 0; q < q_loop_var; q = q + 32) {
                dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
                dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
            }
            dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
            dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
            dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
        }
    }
    //Element-wise Addition
    p_loop_var = p_size;
    r_loop_var = r_size;

    for (p = 0; p < p_loop_var; p++){
        for (r = 0; r < r_loop_var; r =+ 512){
            dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
            dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
            dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
        }
    }

    dma_desc_write(desc_idx, CDMA_DESC_STATUS, 0);
    desc_idx = 0;

    //Matrix-Multiplication START
    p_loop_var = p_size;
    r_loop_var = (r_size * sizeof(Bfloat16)) / 1024;
    q_loop_var = q_size;

    for(p=0;p<p_loop_var;p++){
        for(r=0;r<r_loop_var;r++){
            for(q=0;q<q_loop_var;q=q+32){
                //read A
                A_va = srcA_va + q*sizeof(Bfloat16) + ((q_size*sizeof(Bfloat16)) * p);
                A_pa = va_to_pa(A_va) - pa_offset;
                                                
                dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
                dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
                dma_desc_write(desc_idx, CDMA_DESC_SA_L, A_pa);
                dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
                dma_desc_write(desc_idx, CDMA_DESC_DA_L, PL_DUMMY_PA);
                dma_desc_write(desc_idx, CDMA_DESC_DA_H, 0x0);
                dma_desc_write(desc_idx, CDMA_DESC_LEN, 64);
                dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x2 | (opcode <<4));
                desc_idx++;
                next_desc+=0x40;
                
                //read B
                B_va = srcB_va + (q_size * 1024 * r) + (q * 1024);
                B_pa = va_to_pa(B_va) - pa_offset;

                dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
                dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
                dma_desc_write(desc_idx, CDMA_DESC_SA_L, B_pa);
                dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
                dma_desc_write(desc_idx, CDMA_DESC_DA_L, PL_DUMMY_PA);
                dma_desc_write(desc_idx, CDMA_DESC_DA_H, 0x0);
                dma_desc_write(desc_idx, CDMA_DESC_LEN, 32*1024);
                dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x4 | (opcode <<4));
                desc_idx++;
                next_desc+=0x40;
            }
            //write C
            C_va = dstC_va + (r*1024) + p*r_size*sizeof(Bfloat16);
            C_pa = va_to_pa(C_va) - pa_offset;

            //dummy write barrier
            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, cdma_dev->conf_mem_base);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, cdma_dev->conf_mem_base+1024);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, 1024);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x0 | (opcode <<4));
            desc_idx++;
            next_desc+=0x40;


            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, cdma_dev->conf_mem_base);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, C_pa);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, 1024);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x9 | (opcode <<4));
            desc_idx++;
            next_desc+=0x40;

            //dummy write barrier
            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, cdma_dev->conf_mem_base);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, cdma_dev->conf_mem_base+1024);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, 1024);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x0 | (opcode <<4));
            desc_idx++;
            next_desc+=0x40;

        }
    }
//Matrix-Multiplication END

//Element-wise Addition START
//Src A: bias
//Src B: dst_C
//Dst C: dst_C
    opcode = OPCODE_ELE_ADD;
    p_loop_var = p_size;
    r_loop_var = r_size;
    
    for(p=0;p<p_loop_var;p++){
        for(r=0;r<r_loop_var;r=r+512){

            A_va = bias_va + p*r_size*sizeof(Bfloat16) + r*sizeof(Bfloat16);
            A_pa = va_to_pa(A_va) - pa_offset;

            B_va = dstC_va + p*r_size*sizeof(Bfloat16) + r*sizeof(Bfloat16);
            B_pa = va_to_pa(B_va) - pa_offset;

            C_va = dstC_va + p*r_size*sizeof(Bfloat16) + r*sizeof(Bfloat16);
            C_pa = va_to_pa(C_va) - pa_offset;

            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, A_pa);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, PL_DUMMY_PA);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, 0x0);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, 1024);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x2 | (opcode <<4));
            desc_idx++;
            next_desc+=0x40;

            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, B_pa);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, PL_DUMMY_PA);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, 0x0);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, 1024);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x4 | (opcode <<4));
            desc_idx++;
            next_desc+=0x40;

            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, PL_DUMMY_PA);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, 0);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, C_pa);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, 1024);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x9 | (opcode <<4));
            desc_idx++;
            next_desc+=0x40;
        }
    }


    dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
    dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
    dma_desc_write(desc_idx, CDMA_DESC_SA_L, PL_DUMMY_PA);
    dma_desc_write(desc_idx, CDMA_DESC_SA_H, 0x0);
    dma_desc_write(desc_idx, CDMA_DESC_DA_L, cdma_dev->dram_base + CONF_OFFSET_HPC_CLR);
    dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
    dma_desc_write(desc_idx, CDMA_DESC_LEN, 64);
    dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x0 | (opcode <<4));

    pim_conf_write(CONF_OFFSET_HPC_CLR, 0x0);
    pim_conf_write(CONF_OFFSET_AIM_WORKING, 0x1);
    wmb();
    pim_conf_write(CONF_OFFSET_DESC_BASE, cdma_dev->desc_mem_base);
    pim_conf_write(CONF_OFFSET_DESC_NUM, desc_idx * 64);

    user_info->desc_idx = desc_idx;
    user_info->next_desc = next_desc;

    return ret;
}

int gemm_bias(ioctl_info *user_info, uint32_t opcode)
{
    int ret;
    uint32_t desc_idx, next_desc, p_loop_var, r_loop_var, q_loop_var, p, q, r;
    uint32_t srcA_size, srcB_size, dstC_size, p_size, q_size, r_size;     
    uint32_t A_pa, B_pa, C_pa;
    uint64_t srcA_va, srcB_va, bias_va, dstC_va, A_va, B_va, C_va;
    bool r_need_align;
    uint32_t r_size_aligned;

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
    r_loop_var = (r_size * sizeof(Bfloat16)) / 1024;
    q_loop_var = q_size;

    r_need_align = ( (r_size & 0x1F)) ? 1 : 0;
    //Matrix-Multiplication
    for (p = 0; p < p_loop_var; p++) {
        for(r = 0; r < r_loop_var; r++) {
            for (q = 0; q < q_loop_var; q = q + 32) {
                dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
                dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
            }
            dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
            dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
            dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
        }
    }
    //Element-wise Addition
    q_loop_var = q_size;
    r_loop_var = r_size;


    if (r_need_align) {
        r_size_aligned = (r_size/32+1)*32;
        // printk("q_need_align_size %d\r\n", q_size_aligned);
    }
    else {
        r_size_aligned = r_size;
    }

    for(r = 0; r < r_loop_var; r = r + 512){
        for(q = 0; q < q_loop_var; q++){
            if (q == 0){
                dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
            }
            dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
            dma_desc_write(desc_idx++, CDMA_DESC_STATUS, 0);
        }
    }
    dma_desc_write(desc_idx, CDMA_DESC_STATUS, 0);
    desc_idx = 0;

//Matrix-Multiplication START
    p_loop_var = p_size;
    r_loop_var = (r_size * sizeof(Bfloat16)) / 1024;
    q_loop_var = q_size;

    for (p = 0; p < p_loop_var; p++) {
        for(r = 0; r < r_loop_var; r++) {
            for (q = 0; q < q_loop_var; q = q + 32) {
                //read A
                A_va = srcA_va + q*sizeof(Bfloat16) + ((q_size*sizeof(Bfloat16)) * p);
                A_pa = va_to_pa(A_va) - pa_offset;
                                                
                dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
                dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
                dma_desc_write(desc_idx, CDMA_DESC_SA_L, A_pa);
                dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
                dma_desc_write(desc_idx, CDMA_DESC_DA_L, PL_DUMMY_PA);
                dma_desc_write(desc_idx, CDMA_DESC_DA_H, 0x0);
                dma_desc_write(desc_idx, CDMA_DESC_LEN, 64);
                dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x2 | (opcode <<4));
                desc_idx++;
                next_desc+=0x40;
                
                //read B
                B_va = srcB_va + (q_size * 1024 * r) + (q * 1024);
                B_pa = va_to_pa(B_va) - pa_offset;

                dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
                dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
                dma_desc_write(desc_idx, CDMA_DESC_SA_L, B_pa);
                dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
                dma_desc_write(desc_idx, CDMA_DESC_DA_L, PL_DUMMY_PA);
                dma_desc_write(desc_idx, CDMA_DESC_DA_H, 0x0);
                dma_desc_write(desc_idx, CDMA_DESC_LEN, 32*1024);
                dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x4 | (opcode <<4));
                desc_idx++;
                next_desc+=0x40;
            }
            //write C
            C_va = dstC_va + (r*1024) + p*r_size*sizeof(Bfloat16);
            C_pa = va_to_pa(C_va) - pa_offset;

            //dummy write barrier
            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, cdma_dev->conf_mem_base);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, cdma_dev->conf_mem_base+1024);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, 1024);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x0 | (opcode <<4));
            desc_idx++;
            next_desc+=0x40;


            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, cdma_dev->conf_mem_base);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, C_pa);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, 1024);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x9 | (opcode <<4));
            desc_idx++;
            next_desc+=0x40;

            //dummy write barrier
            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, cdma_dev->conf_mem_base);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, cdma_dev->conf_mem_base+1024);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, 1024);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x0 | (opcode <<4));
            desc_idx++;
            next_desc+=0x40;

        }
    }
//Matrix-Multiplication END

//Element-wise Addition START
//Src A: bias
//Src B: dst_C
//Dst C: dst_C
    opcode = OPCODE_ELE_ADD;
    q_loop_var = 1; //Bias add  1xr_size
    r_loop_var = r_size;

    for(r = 0; r < r_loop_var; r = r + 512){
        for(q = 0; q < q_loop_var; q++){
            if(q==0){

                // A_va = srcA_va + r*sizeof(Bfloat16);
                A_va = bias_va + r*sizeof(Bfloat16);
                A_pa = va_to_pa(A_va);
                dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
                dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
                dma_desc_write(desc_idx, CDMA_DESC_SA_L, A_pa);
                dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
                dma_desc_write(desc_idx, CDMA_DESC_DA_L, PL_DUMMY_PA);
                dma_desc_write(desc_idx, CDMA_DESC_DA_H, 0x0);
                dma_desc_write(desc_idx, CDMA_DESC_LEN, 1024);
                dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x2 | (opcode <<4));
                desc_idx++;
                next_desc+=0x40;
            }
            // B_va = srcB_va + q*r_size*sizeof(Bfloat16) + r*sizeof(Bfloat16);
            // B_va = srcB_va + q*r_size_aligned*sizeof(Bfloat16) + r*sizeof(Bfloat16);
            B_va = dstC_va + q*r_size_aligned*sizeof(Bfloat16) + r*sizeof(Bfloat16);
            B_pa = va_to_pa(B_va);

            // C_va = dstC_va + q*r_size*sizeof(Bfloat16) + r*sizeof(Bfloat16);
            C_va = dstC_va + q*r_size_aligned*sizeof(Bfloat16) + r*sizeof(Bfloat16);
            C_pa = va_to_pa(C_va);
    
            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, B_pa);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, PL_DUMMY_PA);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, 0x0);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, 1024);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x4 | (opcode <<4));
            desc_idx++;
            next_desc+=0x40;


            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, PL_DUMMY_PA);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, 0);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, C_pa);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, 1024);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x9 | (opcode <<4));
            desc_idx++;
            next_desc+=0x40;

        }
    }

    dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
    dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
    dma_desc_write(desc_idx, CDMA_DESC_SA_L, PL_DUMMY_PA);
    dma_desc_write(desc_idx, CDMA_DESC_SA_H, 0x0);
    dma_desc_write(desc_idx, CDMA_DESC_DA_L, cdma_dev->dram_base + CONF_OFFSET_HPC_CLR);
    dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
    dma_desc_write(desc_idx, CDMA_DESC_LEN, 64);
    dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x0 | (opcode <<4));

    pim_conf_write(CONF_OFFSET_HPC_CLR, 0x0);
    pim_conf_write(CONF_OFFSET_AIM_WORKING, 0x1);
    wmb();
    pim_conf_write(CONF_OFFSET_DESC_BASE, cdma_dev->desc_mem_base);
    pim_conf_write(CONF_OFFSET_DESC_NUM, desc_idx * 64);

    return ret;
}