#ifndef AVL_TREE_H
#define AVL_TREE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Use int64_t as the Value.
 */
typedef int64_t TreeValueT;

struct BSTreeNode {
  TreeValueT val;
  struct BSTreeNode *lhs;
  struct BSTreeNode *rhs;
  struct BSTreeNode *parent;
  // Padding to one cache line?
  uint64_t dummy[4];
};

struct BSTree {
  struct BSTreeNode *root;
  // Hidden array of all the nodes.
  struct BSTreeNode *array;
  // Hidden free list of nodes. Nest free is pointed by parent.
  struct BSTreeNode *freelist;
  // Total nodes.
  uint64_t total;
  // Total allocated nodes.
  uint64_t allocated;
};

void buildTreeRecursive(TreeValueT keyMin, TreeValueT keyMax, uint64_t nodes,
                        struct BSTreeNode *root, struct BSTreeNode **freeNodes);

struct BSTree generateUniformTree(TreeValueT keyRange, uint64_t totalNodes) {
  struct BSTreeNode *nodes =
      aligned_alloc(64, sizeof(struct BSTreeNode) * totalNodes);
  struct BSTreeNode **nodes_ptr =
      aligned_alloc(64, sizeof(struct BSTNode *) * totalNodes);

  for (uint64_t i = 0; i < totalNodes; ++i) {
    nodes[i].val = -1;
    nodes[i].lhs = NULL;
    nodes[i].rhs = NULL;
    nodes[i].parent = NULL;
    nodes_ptr[i] = &nodes[i];
  }

  // Shuffle the nodes.
  for (int j = totalNodes - 1; j > 0; --j) {
    int i = (int)(((float)(rand()) / (float)(RAND_MAX)) * j);
    struct BSTreeNode *tmp = nodes_ptr[i];
    nodes_ptr[i] = nodes_ptr[j];
    nodes_ptr[j] = tmp;
  }

  // Recursive build the tree.
  if (keyRange < totalNodes) {
    keyRange = totalNodes;
  }

  struct BSTreeNode *root = nodes_ptr[0];
  TreeValueT rootVal = (keyRange - 1) / 2;
  root->val = rootVal;

  uint64_t lhsNodes = (totalNodes - 1) / 2;
  uint64_t rhsNodes = totalNodes - 1 - lhsNodes;
  buildTreeRecursive(0, rootVal, lhsNodes, root, nodes_ptr + 1);
  buildTreeRecursive(rootVal + 1, keyRange, rhsNodes, root,
                     nodes_ptr + 1 + lhsNodes);

  struct BSTree tree;
  tree.array = nodes;
  tree.root = root;
  tree.freelist = NULL;
  tree.total = totalNodes;
  tree.allocated = totalNodes;
  return tree;
}

void buildTreeRecursive(TreeValueT keyMin, TreeValueT keyMax, uint64_t nodes,
                        struct BSTreeNode *root,
                        struct BSTreeNode **freeNodes) {

  if (nodes == 0) {
    return;
  }

  struct BSTreeNode *node = freeNodes[0];
  TreeValueT nodeVal = (keyMax - keyMin - 1) / 2 + keyMin;
  node->val = nodeVal;
  node->parent = root;
  if (nodeVal < root->val) {
    root->lhs = node;
  } else {
    root->rhs = node;
  }

  uint64_t lhsNodes = (nodes - 1) / 2;
  uint64_t rhsNodes = nodes - 1 - lhsNodes;
  buildTreeRecursive(keyMin, nodeVal, lhsNodes, node, freeNodes + 1);
  buildTreeRecursive(nodeVal + 1, keyMax, rhsNodes, node,
                     freeNodes + 1 + lhsNodes);
}

void generateFilename(char *fn, int bufSize, uint64_t totalNodes,
                      TreeValueT keyMax) {
  snprintf(fn, bufSize, "../bst-%lu-%lu.data", totalNodes, keyMax);
}

void dumpTree(const struct BSTree *tree, TreeValueT keyMax) {
  char fn[256];
  generateFilename(fn, 256, tree->total, keyMax);

  FILE *f = fopen(fn, "wb");
  fwrite(tree, sizeof(*tree), 1, f);
  fwrite(tree->array, sizeof(*tree->array), tree->total, f);
  fclose(f);
}

struct BSTree loadTree(TreeValueT keyMax, uint64_t totalNodes) {
  char fn[256];
  generateFilename(fn, 256, totalNodes, keyMax);

  struct BSTree tree;

  FILE *f = fopen(fn, "rb");
  if (!f) {
    printf("Failed to open tree %s.\n", fn);
    tree.array = NULL;
    return tree;
  }
  fread(&tree, sizeof(tree), 1, f);

  struct BSTreeNode *nodes =
      aligned_alloc(4096, sizeof(struct BSTreeNode) * totalNodes);
  fread(nodes, sizeof(struct BSTreeNode), tree.total, f);

  fclose(f);

#define rewrite_ptr(old_ptr)                                                   \
  (((old_ptr) != NULL) ? (nodes + ((old_ptr)-tree.array)) : NULL)

  // Rewrite all the pointers.
  for (uint64_t i = 0; i < tree.total; ++i) {
    nodes[i].lhs = rewrite_ptr(nodes[i].lhs);
    nodes[i].rhs = rewrite_ptr(nodes[i].rhs);
    nodes[i].parent = rewrite_ptr(nodes[i].parent);
  }

  tree.freelist = rewrite_ptr(tree.freelist);
  tree.root = rewrite_ptr(tree.root);

  // Replace the array.
  tree.array = nodes;

  return tree;
}

void buildTreeRecursiveWithAffnityAlloc(TreeValueT keyMin, TreeValueT keyMax,
                                        uint64_t nodes,
                                        struct BSTreeNode *root) {

  if (nodes == 0) {
    return;
  }

  const void *rootPtr = root;
  struct BSTreeNode *node = malloc_aff(sizeof(struct BSTreeNode), 1, &rootPtr);

  TreeValueT nodeVal = (keyMax - keyMin - 1) / 2 + keyMin;
  node->val = nodeVal;
  node->lhs = NULL;
  node->rhs = NULL;
  node->parent = root;
  if (nodeVal < root->val) {
    root->lhs = node;
  } else {
    root->rhs = node;
  }

  uint64_t lhsNodes = (nodes - 1) / 2;
  uint64_t rhsNodes = nodes - 1 - lhsNodes;
  buildTreeRecursiveWithAffnityAlloc(keyMin, nodeVal, lhsNodes, node);
  buildTreeRecursiveWithAffnityAlloc(nodeVal + 1, keyMax, rhsNodes, node);
}

struct BSTree generateUniformTreeWithAffinityAlloc(TreeValueT keyRange,
                                                   uint64_t totalNodes) {

  if (keyRange < totalNodes) {
    keyRange = totalNodes;
  }

  // Recursive build the tree.
  struct BSTreeNode *root = malloc_aff(sizeof(struct BSTreeNode), 0, NULL);
  TreeValueT rootVal = (keyRange - 1) / 2;
  root->val = rootVal;
  root->lhs = NULL;
  root->rhs = NULL;
  root->parent = NULL;

  uint64_t lhsNodes = (totalNodes - 1) / 2;
  uint64_t rhsNodes = totalNodes - 1 - lhsNodes;
  buildTreeRecursiveWithAffnityAlloc(0, rootVal, lhsNodes, root);
  buildTreeRecursiveWithAffnityAlloc(rootVal + 1, keyRange, rhsNodes, root);

  struct BSTree tree;
  tree.array = NULL;
  tree.root = root;
  tree.freelist = NULL;
  tree.total = totalNodes;
  tree.allocated = totalNodes;
  return tree;
}

#endif