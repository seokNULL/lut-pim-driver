#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h> 
#include <stdint.h>

#include <sys/ioctl.h>
#include <stdbool.h>

#include <pim.h>
#include "convert_numeric.h"

#define PL_DMA "/dev/pl_dma"

struct timespec start_DMA, end_DMA;
struct timespec start_CPU, end_CPU;
uint64_t diff_DMA;
uint64_t diff_CPU;
int iter;

void bias_add_CPU(float *src_A_DRAM, float * src_B_DRAM, float * dst_C_DRAM, int q_size, int r_size)
{
  
  for(int i=0; i<q_size*r_size;i++)
  {
    dst_C_DRAM[i] = src_A_DRAM[i%r_size] + src_B_DRAM[i];
  }
}

void bias_sub_CPU(float *src_A_DRAM, float * src_B_DRAM, float * dst_C_DRAM, int q_size, int r_size)
{
  
  for(int i=0; i<q_size*r_size;i++)
  {
    dst_C_DRAM[i] = src_A_DRAM[i%r_size] - src_B_DRAM[i];
  }
}

void bias_mul_CPU(float *src_A_DRAM, float * src_B_DRAM, float * dst_C_DRAM, int q_size, int r_size)
{
  
  for(int i=0; i<q_size*r_size;i++)
  {
    dst_C_DRAM[i] = src_A_DRAM[i%r_size] * src_B_DRAM[i];
  }
}

int main(int argc, char *argv[])
{

    if(argc<2)
    {
        printf("Check matrix B param q,r (1xr) +-x (qxr) = (qxr)\n");
        exit(1);
    }
    
    int p_size = 1;
    int q_size = atoi(argv[1]);
    int r_size = atoi(argv[2]);

    //int srcA_size = p_size * q_size;
    int srcA_size = p_size * r_size;
    int srcB_size = q_size * r_size;
    int dstC_size = q_size * r_size;

    int tmp=0;

    int fd_dma=0;
    int fd_conf=0;

    if ((fd_dma=open(PL_DMA, O_RDWR|O_SYNC)) < 0) {
        perror("PL_DMA open error");
        exit(-1);
    }

    short *PL_srcA_buf = (short *)pim_malloc(srcA_size*sizeof(short));
    short *PL_srcB_buf = (short *)pim_malloc(srcB_size*sizeof(short));
    short *PL_dstC_buf = (short *)pim_malloc(dstC_size*sizeof(short));

    if (PL_srcA_buf == MAP_FAILED) {
        printf("PL srcA call failure.\n");
        return -1;
    }
    if (PL_srcB_buf == MAP_FAILED) {
        printf("PL srcB call failure.\n");
        return -1;
    }
    if (PL_dstC_buf == MAP_FAILED) {
        printf("PL dstC call failure.\n");
        return -1;
    }

    //zeroing
    for(size_t i=0; i<srcA_size; i++){
      PL_srcA_buf[i]=0;
    }
    for(size_t i=0; i<srcB_size; i++){
      PL_srcB_buf[i]=0;
    }
    for(size_t i=0; i<dstC_size; i++){
      PL_dstC_buf[i]=0;
    }

    //For CPU verify
    float *src_A_DRAM = (float *)(mmap(NULL, srcA_size*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    float *src_B_DRAM = (float *)(mmap(NULL, srcB_size*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    float *dst_C_DRAM = (float *)(mmap(NULL, dstC_size*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

    // Input data
    srand((unsigned int)time(NULL));  // For reset random seed
    
    printf("srcA init\n");
    for(size_t i=0; i<srcA_size; i++){
      float tmp  = generate_random();
      //float tmp  = 0.001625;
      short tmp0 = float_to_short(tmp);
      PL_srcA_buf[i] = tmp0;
      src_A_DRAM[i] = short_to_float(tmp0);
    }

    printf("srcB init\n");
    for(size_t i=0; i<srcB_size; i++){
      float tmp  = generate_random_255();
      //float tmp  = 1;
      //float tmp  = 0.001625;
      short tmp0 = float_to_short(tmp);
      PL_srcB_buf[i]=tmp0;
      src_B_DRAM[i] = short_to_float(tmp0);
    }    
    
    printf("dstC init\n");
    for(size_t i=0; i<dstC_size; i++){
      PL_dstC_buf[i]=0;
      dst_C_DRAM[i] = 0;
    }

    //check CPU result
    printf("check cpu elewise\n");
    // bias_add_CPU(src_A_DRAM, src_B_DRAM, dst_C_DRAM, q_size, r_size);
    //bias_sub_CPU(src_A_DRAM, src_B_DRAM, dst_C_DRAM, q_size, r_size);
    bias_mul_CPU(src_A_DRAM, src_B_DRAM, dst_C_DRAM, q_size, r_size);

    ioctl_info *set_info;
    int size = sizeof(ioctl_info);
    set_info = (ioctl_info *)malloc(size);

    set_info->srcA_ptr  = PL_srcA_buf;
    set_info->srcB_ptr  = PL_srcB_buf;
    set_info->dstC_ptr  = PL_dstC_buf;    
    set_info->srcA_va   = (uint64_t)&PL_srcA_buf[0];
    set_info->srcB_va   = (uint64_t)&PL_srcB_buf[0];
    set_info->dstC_va   = (uint64_t)&PL_dstC_buf[0];
    set_info->srcA_size = srcA_size*sizeof(short);
    set_info->srcB_size = srcB_size*sizeof(short);
    set_info->dstC_size = dstC_size*sizeof(short);
    set_info->p_size    = 1;
    set_info->q_size    = q_size;
    set_info->r_size    = r_size;
    //set_info->dummy_buf_PA = PL_DUMMY_PA;

    clock_gettime(CLOCK_MONOTONIC, &start_DMA);

    if (ioctl(fd_dma, BIAS_MUL, set_info) < 0) {
        printf("ERROR DMA \n");
        return 0;
    }

    clock_gettime(CLOCK_MONOTONIC, &end_DMA);

    diff_DMA = BILLION * (end_DMA.tv_sec - start_DMA.tv_sec) + (end_DMA.tv_nsec - start_DMA.tv_nsec);
    printf("PIM execution time %llu nanoseconds\n", (long long unsigned int) diff_DMA);
    //printf("--------------------------------------\r\n");
    /////////////////////////////////////////////////////////////////////////////////////////////////

    union converter{
    float f_val;
    unsigned int u_val;
    };
    union converter a;
    union converter b;

    printf("Correctness check!\n\n");
    printf("       HOST       |  ARM\n");
    for(int i=0; i<dstC_size; i++){ 
        a.f_val = dst_C_DRAM[i];

        printf("idx[%4d] 0x%x  |  ", i, a.u_val);
        printf("0x%x ", PL_dstC_buf[i]);
        if (abs(dst_C_DRAM[i] - short_to_float(PL_dstC_buf[i])) > 0.02) {
		printf("\tError");
	}
        printf("\n");
    }
    printf("\n");

    pim_free(PL_srcA_buf);
    pim_free(PL_srcB_buf);
    pim_free(PL_dstC_buf);

    return 0;
}




