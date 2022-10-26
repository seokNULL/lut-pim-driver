#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <pim.h>

struct timespec start_t, end_t;
uint64_t diff_time;
#define BILLION 1000000000L

char *ushortToBinary(uint8_t i, int num_page) {
  static char s[8 + 1] = { '0', };
  int count = 8;

  do { s[--count] = '0' + (char) (i & 1);
       i = i >> 1;
  } while (count);

  return s;
}

char *num_to_bin(int num) {
    char *tmp = (char *)malloc(sizeof(char) * num);

    for (size_t i = 0; i < num; i++) {
        tmp[i] = '1';
    }
    return tmp;
}

/* Test code main */
int main()
{
    clock_gettime(CLOCK_MONOTONIC, &start_t);
    diff_time = 0;
    int iter = 5000;
    void *tmp[iter];
    tmp[0] = pim_malloc(3.5*1024*1024);
    //pim_free(tmp[0], 3.5*1024*1024);
    pim_free(tmp[0]);

    tmp[0] = pim_malloc(4*1024*1024);
    //pim_free(tmp[0], 6*1024*1024);    
    pim_free(tmp[0]);
    //exit(1);
    for (int i = 0; i < iter; i++) {
        PIM_MEM_LOG("ITER[%3d] - ", i);
        if (i==0) {
            tmp[i] = pim_malloc(69632);

            for (size_t j = 0; j < 69632/sizeof(int); j++) {
                ((int *)tmp[i])[j] = j % 513;
                if ((((int *)tmp[i])[j]) != (j % 513)) {
                    printf("ERROR");
                    exit(1);
                }
            }
        }
        //else if (i == 1) {
        //    tmp[i] = pim_malloc(4194304);
        //    for (size_t j = 0; j < 4194304/sizeof(int); j++) {
        //        ((int *)tmp[i])[j] = j % 1232;
        //        if ((((int *)tmp[i])[j]) != (j % 1232)) {
        //            printf("ERROR");
        //            exit(1);
        //        }
        //    }
        //}        
        else if (i == 11) {
            tmp[i] = pim_malloc(49152);
            for (size_t j = 0; j < 49152/sizeof(int); j++) {
                ((int *)tmp[i])[j] = j % 1232;
                if ((((int *)tmp[i])[j]) != (j % 1232)) {
                    printf("ERROR");
                    exit(1);
                }
            }
        }
        else if (i == 10) {
            tmp[i] = pim_malloc(69632);

            for (size_t j = 0; j < 69632/sizeof(int); j++) {
                ((int *)tmp[i])[j] = j % 513;
                if ((((int *)tmp[i])[j]) != (j % 513)) {
                    printf("ERROR");
                    exit(1);
                }
            }
            for (size_t j = 0; j < 69632/sizeof(int); j++) {
                ((int *)tmp[0])[j] = j % 513;
                if ((((int *)tmp[0])[j]) != (j % 513)) {
                    printf("ERROR");
                    exit(1);
                }
            }                        
            //pim_free(tmp[0], 69632);
            pim_free(tmp[0]);
            pim_free(tmp[3]);
        }
        else if (i == 110) {
            //pim_free(tmp[55], 300*1024);
            pim_free(tmp[55]);
        }
        else if (i ==51) {
            tmp[i] = pim_malloc(65536);
            for (size_t j = 0; j < 65536/sizeof(int); j++) {
                ((int *)tmp[i])[j] = j % 2212;
                if ((((int *)tmp[i])[j]) != (j % 2212)) {
                    printf("ERROR");
                    exit(1);
                }
            }
        }
        else if (i == 15) {
            tmp[i] = pim_malloc(2097152);
            for (size_t j = 0; j < 2097152/sizeof(int); j++) {
                ((int *)tmp[i])[j] = j % 11222;
                if ((((int *)tmp[i])[j]) != (j % 11222)) {
                    printf("ERROR");
                    exit(1);
                }
            }
            //pim_free(tmp[i], 2097152);
            pim_free(tmp[i]);
        }
        else if (i ==126) {
            tmp[i] = pim_malloc(122880);
        }
        else if (i > 60) {
            tmp[i] = pim_malloc(128*1024);
        }
        else
            tmp[i] = pim_malloc(1*1024);
    }
    //printf("\t\t TMP[11] --> %p %d \n", test, ((int *)test)[0]);

    int *tmp00 = (int *)pim_malloc(65536);
    for (size_t i = 0; i < 65536/sizeof(int); i++) {
        tmp00[i] = i % 1000;
    }
    bool error=false;
    for (size_t i = 0; i < 65536/sizeof(int); i++) {
        if (tmp00[i] != (i % 1000)) {
            error = true;
            break;
        }

    }
    if (error) {
        printf("Error \n");
    } else {
        printf("Success \n");
    }
    //atexit(_real_free);
    clock_gettime(CLOCK_MONOTONIC, &end_t);
    diff_time = BILLION * (end_t.tv_sec - start_t.tv_sec) + (end_t.tv_nsec - start_t.tv_nsec);
    printf("Exe time: %lu\n", diff_time);
    return 0;
}
