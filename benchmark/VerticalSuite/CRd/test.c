#include <stdio.h>
#include "randArr.h"
#include "common.h"

#define ASIZE 2048
#define STEP   128
#define ITERS   20

__attribute__ ((noinline))
int rec(int i){
  if(i==0) return 0;
  if(i==1) return 1;
  if(i<1024) {
    return rec(i-1)+i;
  } else {
    return 5;
  }
}

__attribute__ ((noinline))
int loop(int zero) {
  int t = 0,i,iter;
  for(iter=0; iter < ITERS; ++iter) {
    t+=rec(iter*128);
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
