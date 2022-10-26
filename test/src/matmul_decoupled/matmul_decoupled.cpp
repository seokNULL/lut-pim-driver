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
// #define IMPL_8x4

struct timespec start_DMA, end_DMA;
struct timespec start_CPU, end_CPU;
uint64_t diff_DMA;
uint64_t diff_CPU;
int iter;

void mat_mul_ALU(float *src_A_DRAM, float * src_B_DRAM, float * dst_C_DRAM, int p_size, int q_size, int r_size)
{
  
  union converter{
  float f_val;
  unsigned int u_val;
  };
  union converter a;
  union converter b;
  union converter c;
  union converter d;

  int NUM_BANK = 16;
  int A_ROW = p_size;
  int A_COL = q_size;
  int B_COL = r_size;
  int REG_SIZE = 32;
  int COMPUTE_WAY = 8;
  int A_ROW_PAD, A_COL_PAD, B_COL_PAD;

#ifdef IMPL_8x4
  if (p_size % REG_SIZE == 0) // When p_size is a multiple of 32
#endif
      A_ROW_PAD = (A_ROW + REG_SIZE - 1) / REG_SIZE * REG_SIZE;
#ifdef IMPL_8x4
  else
      A_ROW_PAD = (A_ROW + COMPUTE_WAY - 1) / COMPUTE_WAY * COMPUTE_WAY;
#endif


  A_COL_PAD = (A_COL + REG_SIZE - 1) / REG_SIZE * REG_SIZE;
  B_COL_PAD = (B_COL + NUM_BANK - 1) / NUM_BANK * NUM_BANK;

  int cnt_B = 0;
  int cnt_C = 0;
  int BL = 4;

  float vecA[NUM_BANK][REG_SIZE] = {0,};
  float vecB[NUM_BANK][REG_SIZE] = {0,};
  float accC[NUM_BANK][REG_SIZE] = {0,};
  uint64_t RD_FROM_DRAM_A = 0;
  uint64_t RD_FROM_DRAM_B = 0;
  uint64_t WR_TO_DRAM_C = 0;

  for (size_t io = 0; io < A_ROW_PAD; io += REG_SIZE) {
      int k_dim = 0; // how many 8*4 tiles?
#ifdef IMPL_8x4
      if (io + REG_SIZE > A_ROW_PAD) { // only for 8x4
          k_dim = A_ROW_PAD - (A_ROW / REG_SIZE * REG_SIZE);
      } else {
#endif
          k_dim = REG_SIZE;
#ifdef IMPL_8x4
      }
      int reuse_lev = k_dim / COMPUTE_WAY; // only for 8x4
#endif

      for (size_t jo = 0; jo < B_COL_PAD; jo += NUM_BANK) {
          uint64_t cnt_A = 0;
          // for (size_t ii = 0; ii < REG_SIZE; ii++) {
          for (size_t ko = 0; ko < A_COL_PAD; ko += REG_SIZE) {
              for (size_t bank = 0; bank < NUM_BANK; bank++) {
                  RD_FROM_DRAM_B += REG_SIZE;
                  for (size_t offset = 0; offset < REG_SIZE; offset++) {
                      vecA[bank][offset] = src_B_DRAM[(cnt_B++) % (A_COL_PAD * B_COL_PAD)];
                  }
              }

#ifdef IMPL_8x4
              int reused = 0;
              int vecA_blck = 0; // i'th 4*1 block B tile.
              int elem_cnt = 0;
              int acc_blck = 0;
#endif
              for (size_t k = 0; k < k_dim; k++) {
                  printf("----io=%zd, jo=%zd, ko+k=%zd\n", io, jo, ko + k);
                  // Broadcasting
                  RD_FROM_DRAM_A += REG_SIZE;
                  uint64_t a_addr = (io * A_COL_PAD + cnt_A) % (A_ROW_PAD * A_COL_PAD);
                  cnt_A += REG_SIZE;
                  for (size_t ji = 0; ji < NUM_BANK; ji++) {
                      for (size_t ii = 0; ii < REG_SIZE; ii++) {
                          vecB[ji][ii] = src_A_DRAM[a_addr + ii];
                      }
                  }
#ifdef IMPL_8x4
                  if (p_size % 32 != 0) {
                      // Bank parallel
                      for (size_t ji = 0; ji < NUM_BANK; ji++) {
                          if (ji == 0) {  // Print only for Bank 0.
                            printf("Register values of Bank 0.\n");
                            printf("MatB vecA[%zd] = ", ji);
                            for (size_t ii = 0; ii < REG_SIZE; ii++) {
                                a.f_val = vecA[ji][ii];
                                printf("%x ", a.u_val);
                                // printf("%f ", a.f_val);
                            }
                            printf("\n");
                            printf("MatA vecB[%zd] = ", ji);
                            for (size_t ii = 0; ii < REG_SIZE; ii++) {
                                b.f_val = vecB[ji][ii];
                                printf("%x ", b.u_val);
                                // printf("%f ", b.f_val);
                            }
                            printf("\n\n");
                            printf("ALU inputs of Bank 0\n");
                            printf("MatA=vecB MatB=vecA\n");
                          }

                          for (size_t burst = 0; burst < BL; burst++) {
                              for (size_t alu = 0; alu < COMPUTE_WAY; alu++) {
                                  // if (ji == 0)  // Print Bank 0 only.
                                  b.f_val = vecB[ji][burst * COMPUTE_WAY + alu];
                                  a.f_val = vecA[ji][vecA_blck * BL + elem_cnt];
                                  if(ji==0) printf("%x %x\n", b.u_val, a.u_val);
                                  // printf("%f %f\n", b.f_val, a.f_val);
                                  accC[ji][acc_blck * COMPUTE_WAY + alu] += vecB[ji][burst * COMPUTE_WAY + alu] * vecA[ji][vecA_blck * BL + elem_cnt];
                              }
                              if (++elem_cnt == BL) {
                                  elem_cnt = 0;
                              }
                          }
                          printf("\n");
                      }
                      printf("\n");
                      printf("\n");

                      if (++acc_blck == reuse_lev) {
                          acc_blck = 0;
                      }

                      if (++reused == reuse_lev) {
                          reused = 0;
                          vecA_blck = (++vecA_blck) % COMPUTE_WAY;
                      }
                  } else {
#endif
                      for (size_t ji = 0; ji < NUM_BANK; ji++) {
                          // Register value
                          if (ji == 0) {  // Print only for Bank 0.
                            printf("Register values of Bank 0.\n");
                            printf("MatB vecA[%zd] = ", ji);
                            for (size_t ii = 0; ii < REG_SIZE; ii++) {
                                a.f_val = vecA[ji][ii];
                                printf("%x ", a.u_val);
                                // printf("%f ", a.f_val);
                            }
                            printf("\n");
                            printf("MatA vecB[%zd] = ", ji);
                            for (size_t ii = 0; ii < REG_SIZE; ii++) {
                                b.f_val = vecB[ji][ii];
                                printf("%x ", b.u_val);
                                // printf("%f ", b.f_val);
                            }
                            printf("\n\n");
                            printf("ALU inputs of Bank 0\n");
                            printf("MatA=vecB MatB=vecA\n");
                          }

                          for (size_t ii = 0; ii < REG_SIZE; ii++) {
                              // ALU inputs of Bank 0.
                              if (ji == 0 && ii == 0) {
                                  c.f_val = accC[ji][ii];
                                  b.f_val = vecB[ji][ii];
                                  a.f_val = vecA[ji][k];
                                  printf("%x %x %x\n", c.u_val, b.u_val, a.u_val);
                                //   printf("%f %f\n", b.f_val, a.f_val);
                              }
                              accC[ji][ii] += vecB[ji][ii] * vecA[ji][k];
                          }
                          a.f_val = accC[ji][0];
                          if (ji==0) printf("vAcc[0]=%f %x\n", a.f_val, a.u_val);
                      }
                      printf("\n");
                      printf("\n");
#ifdef IMPL_8x4
                  }
#endif
              }
          }
          printf("cnt_C: %d\n", cnt_C);
          for (size_t bank = 0; bank < NUM_BANK; bank++) {
              WR_TO_DRAM_C += REG_SIZE;
              for (size_t offset = 0; offset < REG_SIZE; offset++) {
                  // dst_C[(io + offset) * B_COL + (jo + bank)] = accC[bank][offset];
                  dst_C_DRAM[(cnt_C++) % (A_ROW_PAD * B_COL_PAD)] = accC[bank][offset];
                  accC[bank][offset] = 0;
              }
          }
      }
  }
}

void mat_mul_CPU(float *src_A_DRAM, float *src_B_DRAM, float *dst_C_DRAM, int p_size, int q_size, int r_size) 
{
    union converter {
        float f_val;
        unsigned int u_val;
    };
    union converter a, b, c, d;

    int A_ROW = p_size;
    int A_COL = q_size;
    int B_COL = r_size;
    for (size_t i = 0; i < A_ROW; i++) {
        for (size_t j = 0; j < B_COL; j++) {
            for (size_t k = 0; k < A_COL; k++) {
                d.f_val = dst_C_DRAM[i * B_COL + j];
                dst_C_DRAM[i * B_COL + j] += src_A_DRAM[i * A_COL + k] * src_B_DRAM[k * B_COL + j];
                if (i == 0 && j == 0) {
                    a.f_val = src_A_DRAM[i * A_COL + k];
                    b.f_val = src_B_DRAM[k * B_COL + j];
                    c.f_val = dst_C_DRAM[i * B_COL + j];
                    printf("%x, a=%x, b=%x, c=%x\n", d.u_val, a.u_val, b.u_val, c.u_val);
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{

    if(argc<2)
    {
        printf("Check matrix param p,q,r (pxq) x (qxr) = (pxr)\n");
        exit(1);
    }
    
    int p_size = atoi(argv[1]);
    int q_size = atoi(argv[2]);
    int r_size = atoi(argv[3]);

    //int cmd = atoi(argv[4]);
    //int type = atoi(argv[5]);

    int srcA_size = p_size * q_size;
    int srcB_size = q_size * r_size;
    int dstC_size = p_size * r_size;

    int A_ROW = p_size;
    int A_COL = q_size;
    int B_COL = r_size;
    int NUM_BANK = 16;
    int REG_SIZE = 32;
    int COMPUTE_WAY = 8;
    int A_ROW_PAD, A_COL_PAD, B_COL_PAD;

#ifdef IMPL_8x4
    if (p_size % REG_SIZE == 0) // When p_size is a multiple of 32
#endif
      A_ROW_PAD = (A_ROW + REG_SIZE - 1) / REG_SIZE * REG_SIZE; // Padding in multiples of 32
#ifdef IMPL_8x4
    else  // p_size가 32 배수가 아닐 때
      A_ROW_PAD = (A_ROW + COMPUTE_WAY - 1) / COMPUTE_WAY *COMPUTE_WAY;
#endif
    A_COL_PAD = (A_COL + REG_SIZE - 1) / REG_SIZE * REG_SIZE;
    B_COL_PAD = (B_COL + NUM_BANK - 1) / NUM_BANK * NUM_BANK;

    int tmp=0;

    int fd_dma=0;
    int fd_conf=0;

    if ((fd_dma=open(PL_DMA, O_RDWR|O_SYNC)) < 0) {
        perror("PL_DMA open error");
        exit(-1);
    }

    //For CPU verify
    float *src_A_DRAM = (float *)(mmap(NULL, srcA_size*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    float *src_B_DRAM = (float *)(mmap(NULL, srcB_size*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    float *dst_C_DRAM = (float *)(mmap(NULL, dstC_size*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    float *src_A_DRAM_ALU = (float *)(mmap(NULL, A_ROW_PAD*A_COL_PAD*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    float *src_B_DRAM_ALU = (float *)(mmap(NULL, A_COL_PAD*B_COL_PAD*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    float *dst_C_DRAM_ALU = (float *)(mmap(NULL, A_ROW_PAD*B_COL_PAD*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

    srand((unsigned int)time(NULL));  // For reset random seed

    void *srcA;
    void *srcB;
    void *dstC;

    uint64_t srcA_va;
    uint64_t srcB_va;
    uint64_t dstC_va;

    ioctl_info *set_info;
    int size = sizeof(ioctl_info);
    set_info = (ioctl_info *)malloc(size);

    short *PL_srcA_buf = (short *)pim_malloc(A_ROW_PAD*A_COL_PAD*sizeof(short));
    short *PL_srcB_buf = (short *)pim_malloc(A_COL_PAD*B_COL_PAD*sizeof(short));
    short *PL_dstC_buf = (short *)pim_malloc(A_ROW_PAD*B_COL_PAD*sizeof(short));


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
    printf("dstC init\n");
    for(size_t i=0; i<dstC_size; i++){
      PL_dstC_buf[i]=0;
      dst_C_DRAM[i] = 0;
      dst_C_DRAM_ALU[i] = 0;
    }

    float cnt = 0;
    size_t idx = 0;
    printf("srcA init\n");

#ifdef IMPL_8x4
      if (p_size % REG_SIZE != 0) { // p_size가 32 배수가 아닐 때
        for (size_t io = 0; io < A_ROW_PAD; io += REG_SIZE) {
            for (size_t ko = 0; ko < A_COL; ko += REG_SIZE / COMPUTE_WAY) {
                for (size_t ii = 0; ii < REG_SIZE && io + ii < A_ROW_PAD; ii += COMPUTE_WAY) {
                    for (size_t ki = 0; ki < REG_SIZE / COMPUTE_WAY; ki++) {
                        for (size_t iii = 0; iii < COMPUTE_WAY; iii++) {
                            float tmp = generate_random();
                            // short tmp0 = float_to_short(tmp);
                            short tmp0 = float_to_short(cnt*0.0001);
                            // printf("i=%d, j=%d, idx=%d\n", (io + ii + iii), ko + ki, idx);
                            PL_srcA_buf[idx] = tmp0;
                            src_A_DRAM[(io + ii + iii) * A_COL + ko + ki] = short_to_float(tmp0);
                            src_A_DRAM_ALU[idx] = short_to_float(tmp0);
                            cnt += 1;
                            idx++;
                        }
                    }
                }
            }
        }
      }
      else {  // When p_size is a multiple of 32
#endif
        ////// input layout
        for (size_t io = 0; io < A_ROW_PAD; io += REG_SIZE) {
            for (size_t k = 0; k < A_COL_PAD; k++) {
                for (size_t ii = 0; ii < REG_SIZE; ii++) {
                    float tmp = generate_random_255();
                    short tmp0 = float_to_short(tmp);
                  //   short tmp0 = float_to_short(cnt*0.001);

                    if (io + ii < A_ROW && k < A_COL) {
                      PL_srcA_buf[idx] = tmp0;
                      src_A_DRAM[(io + ii) * A_COL + k] = short_to_float(tmp0);
                      // src_A_DRAM_ALU[idx] = short_to_float(tmp0);
                    }
                    else {
                      PL_srcA_buf[idx] = 0;
                      // src_A_DRAM_ALU[idx] = 0;
                    }
                    cnt += 1;
                    idx += 1;
                }
            }
        }
#ifdef IMPL_8x4
      }
#endif
    cnt = 0;
    printf("srcB init\n");
    ////// weight layout
    for (size_t jo = 0; jo < B_COL_PAD; jo += 16) {
        for (size_t ko = 0; ko < A_COL_PAD; ko += 32) {
            for (size_t ji = 0; ji < 16; ji++) {
                for (size_t ki = 0; ki < 32; ki++) {
                    // float tmp = generate_random();
                    float tmp = generate_random_255();
                    short tmp0 = float_to_short(tmp);
                    // short tmp0 = float_to_short(cnt*0.001);
                    if (ko + ki < A_COL && jo + ji < B_COL) {
                        PL_srcB_buf[(int)cnt] = tmp0;
                        src_B_DRAM[(ko + ki) * B_COL + (jo + ji)] = short_to_float(tmp0);
                        // src_B_DRAM_ALU[(int)cnt] = short_to_float(tmp0);
                    }
                    else {
                        PL_srcB_buf[(int)cnt] = 0;
                        // src_B_DRAM_ALU[(int)cnt] = 0;
                    }
                    cnt += 1;
                }
            }
        }
    }

    //check CPU result
    printf("check cpu mat mul\n");
    mat_mul_CPU(src_A_DRAM, src_B_DRAM, dst_C_DRAM, p_size, q_size, r_size);
    // mat_mul_ALU(src_A_DRAM_ALU, src_B_DRAM_ALU, dst_C_DRAM_ALU, p_size, q_size, r_size);

    set_info->srcA_ptr  = PL_srcA_buf;
    set_info->srcB_ptr  = PL_srcB_buf;
    set_info->dstC_ptr  = PL_dstC_buf;    
    set_info->srcA_va   = (uint64_t)&PL_srcA_buf[0];
    set_info->srcB_va   = (uint64_t)&PL_srcB_buf[0];
    set_info->dstC_va   = (uint64_t)&PL_dstC_buf[0];
    set_info->srcA_size = srcA_size*sizeof(short);
    set_info->srcB_size = srcB_size*sizeof(short);
    set_info->dstC_size = dstC_size*sizeof(short);
    set_info->p_size    = A_ROW_PAD;
    set_info->q_size    = q_size;
    set_info->r_size    = r_size;

    clock_gettime(CLOCK_MONOTONIC, &start_DMA);

    if (ioctl(fd_dma, MATMUL_DECOUPLED, set_info) < 0) {
         printf("ERROR DMA \n");
         return 0;
    }        

    clock_gettime(CLOCK_MONOTONIC, &end_DMA);  

    diff_DMA = BILLION * (end_DMA.tv_sec - start_DMA.tv_sec) + (end_DMA.tv_nsec - start_DMA.tv_nsec);
    printf("PIM execution time %llu nanoseconds %d %d %d\n", (long long unsigned int) diff_DMA, p_size, q_size, r_size);

    union converter{
    float f_val;
    unsigned int u_val;
    };
    union converter a;
    union converter b;

    cnt = 0;
    ////// result layout
    for (size_t io = 0; io < A_ROW_PAD; io += REG_SIZE) {
        for (size_t j = 0; j < B_COL; j++) {
            for (size_t ii = 0; ii < REG_SIZE; ii++) {
                if (io + ii < A_ROW && j % 16 == 0) {
                    a.f_val = dst_C_DRAM[(io + ii) * B_COL + j];
                    // For mat_mul_ALU verification.
                    b.f_val = dst_C_DRAM_ALU[(int)cnt];
                    printf("%0.f idx[%d] 0x%x || 0x%x || 0x%x\n", cnt, (int)((io + ii) * B_COL + j), a.u_val, b.u_val, PL_dstC_buf[(int)cnt]);
                }
                cnt += 1;
            }
        }
    }

    printf("\n"); 
    pim_free(PL_srcA_buf);
    pim_free(PL_srcB_buf);
    pim_free(PL_dstC_buf);

    return 0;
}



