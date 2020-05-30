#ifndef GEM_FORGE_MICRO_SUITE_GRAPH_UTILS_H
#define GEM_FORGE_MICRO_SUITE_GRAPH_UTILS_H

#include <stdint.h>

typedef uint32_t GraphIndexT;
typedef struct {
  GraphIndexT numNodes, numEdges;
  GraphIndexT *edgePtr; // Pointer into edgeList.
  GraphIndexT *edges;
  GraphIndexT root; // Used for BFS.
} GraphCSR;

GraphCSR readInGraph(const char *fn);

#endif