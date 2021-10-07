#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"

#define MAX_SIZE 100000

#ifndef d1
#define d1 1
#endif

#ifndef d2
#define d2 8
#endif

#ifndef ratio
#define ratio 0.5
#endif

#ifndef B
#define B 1
#endif

struct tree {
  uint64_t *nodeType;
  double *vr;
  double *dr;
  uint64_t *child0;
  uint64_t *child1;
  uint64_t *flag;
};

struct fix_tree {
  uint64_t *nodeType;
  uint64_t *child0;
  uint64_t *child1;
};

struct input_tree {
  double *vr;
  double *dr;
  uint64_t *flag;
};

void forwardPropagation(struct fix_tree ckt, struct input_tree input_ckt[B],
                        int cum_nodes_at_level[d2 - d1], int levels) {
  // pid is parent id
  // printf("came inside forward propagation with levels: %d\n",levels);
  int c0_id, c1_id;
  int d = levels;
  int b = 0;
  // index is the leaf node here? should start from root

  // for (int b = 0; b < B; b++) {
  // for (int d = 0; d < (d2-d1-1); d++) {
  // for (int d = (d2-d1-1); d >= 0; d--) {
  for (d = (d - 1); d >= 0; d--) {
    // printf("working at depth d: %d",d);
    // for (int d = (d-1); d >= 0; d--) {
    for (int pid = cum_nodes_at_level[d]; pid < cum_nodes_at_level[d + 1];
         pid++) {

      c0_id = ckt.child0[pid];
      c1_id = ckt.child1[pid];

      if (ckt.nodeType[pid] == 0) {
        input_ckt[b].flag[pid] = 0;
        input_ckt[b].vr[pid] = 0;
        input_ckt[b].dr[pid] = 0;

        /*Only add values if the flag is down*/
        if (!input_ckt[b].flag[c0_id]) {
          input_ckt[b].vr[pid] += input_ckt[b].vr[c0_id];
        }
        if (!input_ckt[b].flag[c1_id]) {
          input_ckt[b].vr[pid] += input_ckt[b].vr[c1_id];
        }

      } else if (ckt.nodeType[pid] == 1) {

        input_ckt[b].vr[pid] = 1;
        input_ckt[b].dr[pid] = 0;

        /*Raise bit flag if there is exactly one child with value equal to 0*/
        if (input_ckt[b].vr[c0_id] == 0 && input_ckt[b].vr[c1_id] != 0) {
          input_ckt[b].flag[pid] = 1;
          /*Set value to product of all other non-zero child nodes*/
          if (!input_ckt[b].flag[c1_id]) {
            input_ckt[b].vr[pid] = input_ckt[b].vr[c1_id];
          } else {
            input_ckt[b].vr[pid] = 0;
          }
        } else if (input_ckt[b].vr[c0_id] != 0 && input_ckt[b].vr[c1_id] == 0) {
          input_ckt[b].flag[pid] = 1;
          /*Set value to product of all other non-zero child nodes*/
          if (!input_ckt[b].flag[c0_id]) {
            input_ckt[b].vr[pid] = input_ckt[b].vr[c0_id];
          } else {
            input_ckt[b].vr[pid] = 0;
          }
        } else {
          input_ckt[b].flag[pid] = 0;
          if (!input_ckt[b].flag[c0_id]) {
            input_ckt[b].vr[pid] *= input_ckt[b].vr[c0_id];
          } else {
            input_ckt[b].vr[pid] = 0;
          }
          if (!input_ckt[b].flag[c1_id]) {
            input_ckt[b].vr[pid] *= input_ckt[b].vr[c1_id];
          } else {
            input_ckt[b].vr[pid] = 0;
          }
        }
      }
    }
  }
}

void backPropagation(struct fix_tree ckt, struct input_tree input_ckt[B],
                     int cum_nodes_at_level[d2 - d1]) {
  // pid is parent id
  int c0_id, c1_id;
  // index is the leaf node here? should start from root

  for (int b = 0; b < B; b++) {
    for (int d = 0; d < (d2 - d1 - 1); d++) {
      // add an extra condition here that total nodes till here is
      // total_nodes/C
      for (int pid = cum_nodes_at_level[d]; pid < cum_nodes_at_level[d + 1];
           pid++) {
        c0_id = ckt.child0[pid];
        c1_id = ckt.child1[pid];

        if (ckt.nodeType[pid] == 0) {
          input_ckt[b].dr[c0_id] = input_ckt[b].dr[pid];
          input_ckt[b].dr[c1_id] = input_ckt[b].dr[pid];
        } else if (ckt.nodeType[pid] == 1) {
          if (input_ckt[b].dr[pid] == 0) {
            input_ckt[b].dr[c0_id] = 0;
            input_ckt[b].dr[c1_id] = 0;
          } else if (input_ckt[b].flag[pid]) {
            if (input_ckt[b].vr[c0_id] == 0) {
              input_ckt[b].dr[c0_id] =
                  input_ckt[b].dr[pid] * input_ckt[b].vr[pid];
              input_ckt[b].dr[c1_id] = 0;
            } else {
              input_ckt[b].dr[c0_id] =
                  input_ckt[b].dr[pid] *
                  (input_ckt[b].vr[pid] / input_ckt[b].vr[c0_id]);
              input_ckt[b].dr[c1_id] = 0;
            }
          } else {
            input_ckt[b].dr[c0_id] =
                input_ckt[b].dr[pid] *
                (input_ckt[b].vr[pid] / input_ckt[b].vr[c0_id]);
            input_ckt[b].dr[c1_id] =
                input_ckt[b].dr[pid] *
                (input_ckt[b].vr[pid] / input_ckt[b].vr[c1_id]);
          }
        }
      }
    }
  }
}

int main() {
  FILE *ac_file;
  int nodes_at_level[d2 - d1];
  int cum_nodes_at_level[d2 - d1];
  int cur_level = 0;
  struct fix_tree ckt;
  struct input_tree input_ckt[B];
  int index = 0;
  int *a, *b, *c, *d;

  ckt.nodeType = (uint64_t *)malloc(sizeof(uint64_t) * MAX_SIZE);
  ckt.child0 = (uint64_t *)malloc(sizeof(uint64_t) * MAX_SIZE);
  ckt.child1 = (uint64_t *)malloc(sizeof(uint64_t) * MAX_SIZE);

  for (int i = 0; i < B; ++i) {
    input_ckt[i].vr = (double *)malloc(sizeof(double) * MAX_SIZE);
    input_ckt[i].dr = (double *)malloc(sizeof(double) * MAX_SIZE);
    input_ckt[i].flag = (uint64_t *)malloc(sizeof(uint64_t) * MAX_SIZE);
  }

  a = (int *)malloc(sizeof(int) * MAX_SIZE);
  b = (int *)malloc(sizeof(int) * MAX_SIZE);
  c = (int *)malloc(sizeof(int) * MAX_SIZE);
  d = (int *)malloc(sizeof(int) * MAX_SIZE);

  ac_file = fopen("input.data", "r");

  printf("Started reading file!\n");

  char level[16];
  char op;
  int n;
  while (fscanf(ac_file, "%s %d %c", level, &n, &op) == 3) {

    nodes_at_level[cur_level] = n;
    if (cur_level == 0) {
      cum_nodes_at_level[cur_level] = n;
    } else {
      cum_nodes_at_level[cur_level] = (n + cum_nodes_at_level[cur_level - 1]);
    }
    char ignore;
    for (int i = 0; i < n * 2; i += 2) {
      fscanf(ac_file, "%u %c %u %u %c %u", &a[index], &ignore, &b[index],
             &c[index], &ignore, &d[index]);
      if (op == '*') {
        ckt.nodeType[index] = 1;
      } else {
        ckt.nodeType[index] = 0;
      }
      for (int i = 0; i < B; ++i) {
        input_ckt[i].vr[index] = rand();
        input_ckt[i].dr[index] = rand();
        input_ckt[i].flag[index] = rand() % 2;
      }
      index++;
    }
    cur_level++;
  }

  int child1_ind, child2_ind;
  for (int i = 0; i < index; i++) {
    child1_ind = cum_nodes_at_level[a[i] - 1] + b[i];
    child2_ind = cum_nodes_at_level[c[i] - 1] + d[i];

    ckt.child0[i] = child1_ind;
    ckt.child1[i] = child2_ind;
  }

  // int no_nodes_per_cgra = 1484;
  // int no_nodes_per_core = index/(cores_per_spad*num_pe);
  // int no_nodes_per_core = index/cores_per_spad;
  // int no_nodes_per_core = 2192/cores_per_spad+1;
  // int no_nodes_per_core = 3340/cores_per_spad+1;
  //   int no_nodes_per_core = 1484 / cores_per_spad + 1;

  printf("Done reading file, nodes %u!\n", index);

  int levels = d2 - d1 - 1;
  // turn it off just to check forward propagation quickly
  //   for (int i = 0; i < (d2 - d1 - 1); i++) {
  //     if (cum_nodes_at_level[i] > no_nodes_per_core) {
  //       cum_nodes_at_level[i] = no_nodes_per_core;
  //       levels = i + 1;
  //       break;
  //     }
  //   }
  // for(int i=(d2-d1-2); i>=0; i--){
  //   if(cum_nodes_at_level[i]>no_nodes_per_core){
  //     cum_nodes_at_level[i]=no_nodes_per_core;
  //     levels=i+1;
  //     break;
  //   }
  // }

  forwardPropagation(ckt, input_ckt, cum_nodes_at_level, levels);

  backPropagation(ckt, input_ckt, cum_nodes_at_level);
  return 0;
}