#include <stdio.h>
#include "randArr.h"
#include "common.h"
#define ASIZE 65536
#define STEP   128
#define ITERS   32

int arr[ASIZE];

__attribute__ ((noinline))
int loop(int zero) {
  int t = 0,i,iter;
  for(iter=zero; iter < ITERS; ++iter) {
    for(i=iter; i < ASIZE; i+=1024) {
      t += arr[i+0*128];
      t += arr[i+1*128];
      t += arr[i+2*128];
      t += arr[i+3*128];
      t += arr[i+4*128];
      t += arr[i+4*128+1];
      t += arr[i+6*128];
      t += arr[i+6*128+1];
    }
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
