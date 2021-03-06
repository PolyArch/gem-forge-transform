#include <stdio.h>
#include <stdlib.h>     /* malloc, free, rand */

#include "randArr.h"
#include "common.h"

#define ASIZE 2048
#define STEP   128
#define ITERS    2
#define LEN  32768

int arr[ASIZE];

//make sure struct occupies an entire cache line.
struct ll {
  int val;
  long long p1,p2,p3,p4,p5,p6,p7,p8,p9,p10;
  long long q1,q2,q3,q4,q5,q6,q7,q8,q9,q10;
  struct ll* _next;
};

__attribute__ ((noinline))
int loop(int zero,struct ll* n) {
  int t = 0,i,iter;
  for(iter=0; iter < ITERS; ++iter) {
    struct ll* cur =n;
    while(cur!=NULL) {
      t+=cur->val;
      cur=cur->_next;
    }
  }
  return t;
}

int main(int argc, char* argv[]) {
   argc&=10000;
   struct ll *n, *cur;

   int i;
   n=malloc(sizeof(struct ll));
   cur=n;
   for(i=0;i<LEN;++i) {
     cur->val=i;
     cur->_next=malloc(sizeof(struct ll));
     cur=cur->_next;
   }
   cur->val=100;
   cur->_next=NULL;

   m5_detail_sim_start(); 
   int t=loop(argc,n);
   m5_detail_sim_end();

   volatile int a = t;
}

