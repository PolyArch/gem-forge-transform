#include <stdio.h>
#include "randArr.h"
#include "common.h"

#define ASIZE 2048
#define STEP   128
#define ITERS 4096

__attribute__ ((noinline))
int loop(int zero) {
  int t1 = 1;
  int t2 = 89;
  int t3 = 3;
  int t4 = 21;
  int t5 = 2;
  int t6 = 7;
  int t7 = 2;
  int t8 = 3;
  int t9 = 1;
  int t10 = 89;
  int t11 = 3;
  int t12 = 21;
  int t13 = 2;
  int t14 = 7;
  int t15 = 2;
  int t16 = 3;

  int i;

  for(i=zero; i < ITERS; i+=1) {
    t1=(t1 | 4) + 11;
    t2=(t2 ^ 1) + 2;
    t3=(~t3 | 9) ^ 6;
    t4=(~t4 ^ 13) & 3;
    t5=(~t5 + 10) | 1;
    t6=(~t6 - 2) & 5;
    t7=(t7 | 8) & 5;
    t8=(~t8 << 5) ^ 1;
    t9=(~t9 ^ 12) | 8;
    t10=(t10 | 7) & 11;
    t11=(~t11 & 11) | 1;
    t12=(t12 | 11) - 11;
    t13=(~t13 & 5) - 8;
    t14=(t14 - 5) ^ 2;
    t15=(t15 << 4) | 10;
    t16=(~t16 | 8) << 12;

    /** INFO: -O3 optimizes away loop
    t1&=i;
    t2&=i;
    t3&=i;
    t4&=i;
    t5&=i;
    t6&=i;
    t7&=i;
    t8&=i;
    t9&=i;
    t10&=i;
    t11&=i;
    t12&=i;
    t13&=i;
    t14&=i;
    t15&=i;
    */
  }
  return t1+t2+t3+t4+t5+t6+t7+t8*t9+t10+t11+t12+t13+t14+t15+t16;
}

int main(int argc, char* argv[]) {
   argc&=10000;
   m5_detail_sim_start(); 
   int t=loop(argc); 
   m5_detail_sim_end();
   volatile int a = t;
}

/** INFO: Compute autogen with Python

from random import choice, randint
ops = ['+', '-', '&', '|', '^', '<<', '>>']
mods = ['', '~']
def gen_seq(num, rand1, rand2):
  for i in range(num):
    var = f't{i+1}'
    c1 = randint(1, rand1)
    c2 = randint(1, rand2)
    op1 = choice(ops)
    op2 = choice(ops)
    mod = choice(mods)
    print("{var}=({var} {op1} {c1}) {op2} {c2};")
gen_seq(16, 13, 13)
*/
