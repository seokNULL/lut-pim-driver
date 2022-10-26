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


int matmul_decoupled(ioctl_info *user_info, uint32_t opcode)
{
    int ret;
	uint32_t desc_idx, next_desc, p, q, r;
	uint32_t srcA_size, srcB_size, dstC_size, p_size, q_size, r_size;
    uint32_t A_pa, B_pa, C_pa;
	uint64_t srcA_va, srcB_va, bias_va, dstC_va, A_va, B_va, C_va;

	/* For decoupled variables */
    uint32_t A_ROW_PAD, cnt_B, cnt_C, Decoupled_config;

    PL_DMA_LOG("Decoupled Mat-mul (OPCODE: %d)", opcode);

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

	A_ROW_PAD = (p_size + COMPUTE_WAY - 1) / COMPUTE_WAY * COMPUTE_WAY;
	cnt_B = 0;
	cnt_C = 0;
	Decoupled_config = 0;

    for (p = 0; p < p_size; p += REG_SIZE) {
        for (r = 0; r < r_size; r += NUM_BANKS) {
            for (q = 0; q < q_size; q += REG_SIZE) {
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

    /*
     * Execution as 32x1 only when p_size is a multiple of 32
     * In all other cases, it executes in 8x4
     * The code below is the same
     */
    if (p_size % REG_SIZE != 0)
        opcode = OPCODE_MAT_MUL_DECOUPLED_8x4;

    for (p = 0; p < p_size; p += REG_SIZE) {
        int p_size_tile = REG_SIZE; // how many 8*4 tiles?
        if (p + REG_SIZE > A_ROW_PAD) {
            p_size_tile = A_ROW_PAD - (p_size / REG_SIZE * REG_SIZE);
            // TODO: config @ when p of the remaining tile is less than 32.
            switch (p_size_tile) {
                case 8:  Decoupled_config = 0; break;
                case 16: Decoupled_config = 1; break;
                case 24: Decoupled_config = 2; break;
                case 32: Decoupled_config = 3; break;
            }
        }
        for (r = 0; r < r_size; r += NUM_BANKS) {
            uint64_t cnt_A = 0;

            for (q = 0; q < q_size; q += REG_SIZE) {
                B_va = srcB_va + ((cnt_B) % (q_size * r_size)) * TYPE_SIZE;
                B_pa = va_to_pa(B_va) - pa_offset;
                cnt_B += 512;

                dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
                dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
                dma_desc_write(desc_idx, CDMA_DESC_SA_L, B_pa);
                dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
                dma_desc_write(desc_idx, CDMA_DESC_DA_L, PL_DUMMY_PA);
                dma_desc_write(desc_idx, CDMA_DESC_DA_H, 0);
                dma_desc_write(desc_idx, CDMA_DESC_LEN, REG_SIZE * NUM_BANKS * TYPE_SIZE);
                dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x2 | (opcode << 4) | (Decoupled_config << 22));
                desc_idx++;
                next_desc += 0x40;

                A_va = srcA_va + ((p * q_size + cnt_A) % (p_size * q_size)) * TYPE_SIZE;
                A_pa = va_to_pa(A_va) - pa_offset;
                cnt_A += REG_SIZE * p_size_tile;

                dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
                dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
                dma_desc_write(desc_idx, CDMA_DESC_SA_L, A_pa);
                dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
                dma_desc_write(desc_idx, CDMA_DESC_DA_L, PL_DUMMY_PA);
                dma_desc_write(desc_idx, CDMA_DESC_DA_H, 0);
                dma_desc_write(desc_idx, CDMA_DESC_LEN, REG_SIZE * p_size_tile * TYPE_SIZE);
                dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x4 | (opcode << 4) | (Decoupled_config << 22));
                desc_idx++;
                next_desc += 0x40;
            }
            C_va = dstC_va + (cnt_C) * TYPE_SIZE;
            C_pa = va_to_pa(C_va) - pa_offset;
            cnt_C += REG_SIZE * NUM_BANKS;

            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, cdma_dev->conf_mem_base);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, cdma_dev->conf_mem_base+(vACC_SIZE * NUM_BANKS * TYPE_SIZE));
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, vACC_SIZE * NUM_BANKS * TYPE_SIZE);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x0 | (opcode <<4) | (Decoupled_config << 22));
            desc_idx++;
            next_desc+=0x40;

            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, PL_DUMMY_PA);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, 0);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, C_pa);
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, REG_SIZE * NUM_BANKS * TYPE_SIZE);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x9 | (opcode << 4) | (Decoupled_config << 22));

            desc_idx++;
            next_desc += 0x40;

            //dummy write barrier
            dma_desc_write(desc_idx, CDMA_NEXT_DE_L, next_desc);
            dma_desc_write(desc_idx, CDMA_NEXT_DE_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_SA_L, cdma_dev->conf_mem_base);
            dma_desc_write(desc_idx, CDMA_DESC_SA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_DA_L, cdma_dev->conf_mem_base+(vACC_SIZE * NUM_BANKS * TYPE_SIZE));
            dma_desc_write(desc_idx, CDMA_DESC_DA_H, HIGH_ADDR);
            dma_desc_write(desc_idx, CDMA_DESC_LEN, vACC_SIZE * NUM_BANKS * TYPE_SIZE);
            dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x0 | (opcode <<4) | (Decoupled_config << 22));
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
    dma_desc_write(desc_idx, CDMA_DESC_INFO, 0x0 | (opcode << 4) | (Decoupled_config << 22));

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
