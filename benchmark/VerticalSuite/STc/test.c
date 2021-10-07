#include <stdio.h>
#include "randArr.h"
#include "common.h"

#define ASIZE 2048
#define STEP  1024
#define ITERS   16

int arr[ASIZE];

__attribute__ ((noinline))
int loop(int zero) {
  int t = 0,i,iter;
  for(iter=0; iter < ITERS; ++iter) {
    for(i=zero; i < STEP; ++i) {
      arr[i]=i; 
    }
    t+=arr[ASIZE-1-zero];
  }
  return t;
}

int main(int argc, char* argv[]) {
   argc&=10000;
   m5_detail_sim_start(); 
   int t=loop(argc); 
   m5_detail_sim_end();
   volatile int a = t;
}
