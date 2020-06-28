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

typedef uint32_t WeightT;
typedef struct {
  GraphIndexT destVertex;
  WeightT weight;
} WeightEdgeT;

typedef struct {
  GraphIndexT numNodes, numEdges;
  GraphIndexT *edgePtr; // Pointer into edgeList.
  WeightEdgeT *edges;
  GraphIndexT root; // Used for BFS.
} WeightGraphCSR;

WeightGraphCSR readInWeightGraph(const char *fn);

#endif