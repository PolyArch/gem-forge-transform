#include <stdio.h>
#include "randArr.h"
#include "common.h"

#define ASIZE 2048
#define STEP   256
#define ITERS   32

__attribute__ ((noinline))
int loop(int zero) {
  int t = 0,i,iter;
  for(iter=0; iter < ITERS; ++iter) {
    for(i=zero; i < STEP; i+=1) {
      if(i<zero)  {
        t+=3+3*t;
      } else {
        t-=1-5*t;
      }
    }
  }
  return t;
}

int main(int argc, char* argv[]) {
   argc&=10000;
   DETAILED_SIM_START();
   int t=loop(argc);
   DETAILED_SIM_STOP();
   volatile int a = t;
}
