#ifndef _PIM_H_
#define _PIM_H_

/* For PIM memory management */
//#define DEBUG_PIM_MEM /* Enable debugging log */
#define __PIM_MALLOC__ /* if this define is disabled, pim_malloc is changed to standard malloc function */
#define __FPGA__ /* if this define is enabled, PIM driver MUST BE inserted into the kernel */

/* For DMA */
#define INTR_ENABLE /* Enable interrupt. If this define is disabled, polling mode */
#define DEBUG_PL_DMA /* Xilinx CDMA debugging log */
//#define PERF_TIME /* For time measurement */

typedef struct
{
	const void *srcA_ptr;
	const void *srcB_ptr;
	const void *srcbias_ptr;
	void *dstC_ptr;

	uint64_t srcA_va;
	uint64_t srcB_va;
	uint64_t bias_va;
	uint64_t dstC_va;

	int srcA_size;
	int srcB_size;
	int dstC_size;

	int p_size;
	int q_size;
	int r_size;

	uint32_t desc_idx;
	uint32_t next_desc;
	uint32_t length;

	uint64_t dma_tx;
    void * dma_tx_ptr;
} __attribute__ ((packed)) ioctl_info;

typedef uint16_t Bfloat16;


#define PL_DMA_DRV "/dev/pl_dma"
#define PS_DMA_DRV "/dev/ps_dma"
#define X86_DMA_DRV "/dev/x86_dma"

#define PL_DRV_NAME "pl_dma"
#define PS_DRV_NAME "ps_dma"
#define HOST_DRV_NAME "x86_dma"
#define PIM_CONF_DRV_NAME "pim_conf_reg"

#define PL_DMA_REG_NAME "reg-region"
#define PL_DMA_DESC_NAME "desc-region"
#define CONF_BRAM_NAME "conf-bram-region"
#define CONF_REG_NAME "conf-reg-region"

#define PIM_MEM "pim-mem-region"
#define DRIVER_NAME "pim_mmap_dev"

/* 
 * For PL dma ioctl 
 * @ Number 0 ~ 18: PL DMA
 * @ Number 19 ~: Python interface for performance measurement
 */
#define MATMUL       			0
#define MATMUL_DECOUPLED        1
#define ELEWISE                 2
#define ELEWISE_ADD             3
#define ELEWISE_SUB             4
#define ELEWISE_MUL             5
#define ACTIVE                  6
#define ACTIVE_SIGMOID          7
#define ACTIVE_TANH             8
#define BIAS_OP   	   		    9
#define BIAS_ADD  				10
#define BIAS_SUB          		11
#define BIAS_MUL			 	12
#define GEMM                    13
#define GEMM_BIAS               14
#define MEMCPY_PL2PL          	15
#define MEMCPY_PL2PS          	16
#define MEMCPY_PS2PL			17
#define TIME_INIT	        	18
#define GET_VM_TIME	    		29
#define GET_EWAdd_TIME	    	20
#define GET_EWMul_TIME	    	21
#define GET_EWSub_TIME	    	22
#define GET_DESC_TIME	    	23
#define GET_CONFIG_TIME			24
#define GET_CNT_CONFG			25

#define LUT_OPS			        26

/* 
 * For x86 host dma ioctl 
 */
#define HOST2HOST             	0
#define HOST2FPGA             	1
#define FPGA2HOST             	2
#define H2H_ZCPY              	3
#define BILLION 1000000000L


/* For PIM memory library */
#ifdef __cplusplus
extern "C"
{      /* Assume C declarations for C++ */
#endif /* __cplusplus */

#ifdef DEBUG_PIM_MEM
#define PIM_MEM_LOG(fmt, ...) printf(fmt, ##__VA_ARGS__);
#else
#define PIM_MEM_LOG(fmt, ...)
#endif

/* DO NOT USE va_to_pa call in userspace */
#define VA_TO_PA(va) va
/* 
 * External functions for PIM memory shared library
 */
extern void *pim_malloc(size_t size);
extern void pim_free(void *addr);
extern void _real_free(void);

#ifdef __cplusplus
}  /* Assume C declarations for C++ */
#endif /* __cplusplus */
#endif
