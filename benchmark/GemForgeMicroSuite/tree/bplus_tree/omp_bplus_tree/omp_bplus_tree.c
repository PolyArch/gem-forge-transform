
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef int64_t TreeValueT;

const int64_t Degree = 4;
const int64_t PageSize = 4096;
const int64_t LineSize = 64;

struct BPlusTreeNode {
  /**
   * Key assumption:
   * 1. No node is empty (at least one key).
   * 2. Internal node has MIN, MAX nodes.
   */
  int numKeys;
  int isLeaf;
  struct BPlusTreeNode *parent;
  TreeValueT keys[Degree + 1];
  struct BPlusTreeNode *children[Degree + 1];
};

struct BPlusTree {
  struct BPlusTreeNode *root;
  int height;
};

struct BPlusTree initEmptyBPlusTree() {
  struct BPlusTree tree;

  tree.root = aligned_alloc(PageSize, sizeof(struct BPlusTreeNode));
  tree.root->numKeys = 0;
  tree.root->parent = NULL;
  tree.root->isLeaf = 1;
  tree.height = 1;

  return tree;
}

struct BPlusTreeNode *queryToLeaf(struct BPlusTree *tree, TreeValueT target) {
  int height = tree->height;
  struct BPlusTreeNode *curNode = tree->root;

  // Search the internal nodes.
  for (int64_t i = 0; i + 1 < height; ++i) {
    int64_t j = 0;
    // The last key in internal nodes are dummy MAX with NULL child.
    for (; j + 1 < curNode->numKeys; ++j) {
      TreeValueT lhsKey = curNode->keys[j + 0];
      TreeValueT rhsKey = curNode->keys[j + 1];
      int matched = lhsKey <= target && target < rhsKey;
      if (matched) {
        break;
      }
    }
    curNode = curNode->children[j];
  }

  return curNode;
}

void *queryKey(struct BPlusTree *tree, TreeValueT target) {

  struct BPlusTreeNode *leaf = queryToLeaf(tree, target);

  // Search the leaf.
  int numKeys = leaf->numKeys;

#ifdef DEBUG
  printf("[Query] Leaf %p Keys %d Target %lx.\n", leaf, numKeys, target);
  for (int64_t i = 0; i < numKeys; ++i) {
    printf("[Query]  %ld %lx %p.\n", i, leaf->keys[i], leaf->children[i]);
  }
#endif

  for (int64_t i = 0; i < numKeys; ++i) {
    TreeValueT key = leaf->keys[i];
    if (key == target) {
      return leaf->children[i];
    }
  }
  return NULL;
}

void insertToNode(struct BPlusTreeNode *node, TreeValueT target,
                  struct BPlusTreeNode *child) {
  int64_t i = 0;
  // Search for the insert point.
  for (; i < node->numKeys; ++i) {
    TreeValueT key = node->keys[i];
    if (key > target) {
      break;
    }
  }
  // Insert and shift to right.
  TreeValueT curKey = target;
  struct BPlusTreeNode *curRecord = child;
  for (; i <= node->numKeys; ++i) {
    TreeValueT key = node->keys[i];
    node->keys[i] = curKey;
    curKey = key;

    struct BPlusTreeNode *child = node->children[i];
    node->children[i] = curRecord;
    curRecord = child;
  }

  node->numKeys++;

#ifdef DEBUG
  printf("[Insert] Node %p Key %lx Value %p.\n", node, target, child);
  for (int64_t i = 0; i < node->numKeys; ++i) {
    printf("[Insert]   %ld %lx %p.\n", i, node->keys[i], node->children[i]);
  }
#endif
}

void insertKey(struct BPlusTree *tree, TreeValueT target, void *record) {
  struct BPlusTreeNode *node = queryToLeaf(tree, target);

  insertToNode(node, target, record);

  // Check if we need to split.
  while (node->numKeys == Degree + 1) {
    struct BPlusTreeNode *rhsNode =
        aligned_alloc(PageSize, sizeof(struct BPlusTreeNode));

    int64_t split = (Degree + 1) / 2;
    TreeValueT splitKey = node->keys[split];
    rhsNode->isLeaf = node->isLeaf;
    rhsNode->numKeys = Degree + 1 - split;
    node->numKeys = node->isLeaf ? split : split + 1;
    for (int64_t i = 0; i < Degree + 1 - split; ++i) {
      rhsNode->keys[i] = node->keys[i + split];
      rhsNode->children[i] = node->children[i + split];
      if (!rhsNode->isLeaf && rhsNode->children[i]) {
        rhsNode->children[i]->parent = rhsNode;
      }
    }

#ifdef DEBUG
    printf("[Split] Node %p %d-%d Split %ld.\n", node, node->numKeys,
           rhsNode->numKeys, split);
    for (int64_t i = 0; i < node->numKeys; ++i) {
      printf("[Split]   LHS %ld %lx %p.\n", i, node->keys[i],
             node->children[i]);
    }
    for (int64_t i = 0; i < rhsNode->numKeys; ++i) {
      printf("[Split]   RHS %ld %lx %p.\n", i, rhsNode->keys[i],
             rhsNode->children[i]);
    }

#endif

    struct BPlusTreeNode *parent = node->parent;
    if (!parent) {
      parent = aligned_alloc(PageSize, sizeof(struct BPlusTreeNode));
      parent->isLeaf = 0;
      parent->parent = NULL;
      parent->numKeys = 2;
      parent->keys[0] = INT64_MIN;
      parent->keys[1] = INT64_MAX;
      parent->children[0] = node;
      parent->children[1] = NULL;
      tree->root = parent;
      tree->height++;
      node->parent = parent;
#ifdef DEBUG
      printf("[Parent] Node %p.\n", parent);
#endif
    }

    rhsNode->parent = parent;
    insertToNode(parent, splitKey, rhsNode);
    node = parent;
  }
}

struct BPlusTree bulkLoad(int64_t N, TreeValueT *keys, void **values) {
  // Assume keys are sorted.
  struct BPlusTree tree = initEmptyBPlusTree();

  struct BPlusTreeNode *curLeaf = tree.root;
  for (int64_t i = 0; i < N; ++i) {
    int curLeafKeys = curLeaf->numKeys;
    curLeaf->keys[curLeafKeys] = keys[i];
    curLeaf->children[curLeafKeys] = values[i];
    curLeaf->numKeys++;
#ifdef DEBUG
    printf("[BULK] Add to Node %p Keys %d %ld %lx %p.\n", curLeaf, curLeafKeys, i,
           keys[i], values[i]);
#endif

    struct BPlusTreeNode *node = curLeaf;
    // Check if we need to split.
    while (node->numKeys == Degree + 1) {
      struct BPlusTreeNode *rhsNode =
          aligned_alloc(PageSize, sizeof(struct BPlusTreeNode));

      // Update to the next leaf.
      if (node->isLeaf) {
        curLeaf = rhsNode;
      }

      int64_t split = node->isLeaf ? Degree : Degree - 1;
      TreeValueT splitKey = node->keys[split];
      rhsNode->isLeaf = node->isLeaf;
      rhsNode->numKeys = Degree + 1 - split;
      node->numKeys = node->isLeaf ? split : split + 1;
      for (int64_t i = 0; i < Degree + 1 - split; ++i) {
        rhsNode->keys[i] = node->keys[i + split];
        rhsNode->children[i] = node->children[i + split];
        if (!rhsNode->isLeaf && rhsNode->children[i]) {
          rhsNode->children[i]->parent = rhsNode;
        }
      }

#ifdef DEBUG
      printf("[Split] Node %p %d-%d Split %ld.\n", node, node->numKeys,
             rhsNode->numKeys, split);
      for (int64_t i = 0; i < node->numKeys; ++i) {
        printf("[Split]   LHS %ld %lx %p.\n", i, node->keys[i],
               node->children[i]);
      }
      for (int64_t i = 0; i < rhsNode->numKeys; ++i) {
        printf("[Split]   RHS %ld %lx %p.\n", i, rhsNode->keys[i],
               rhsNode->children[i]);
      }

#endif

      struct BPlusTreeNode *parent = node->parent;
      if (!parent) {
        parent = aligned_alloc(PageSize, sizeof(struct BPlusTreeNode));
        parent->isLeaf = 0;
        parent->parent = NULL;
        parent->numKeys = 2;
        parent->keys[0] = INT64_MIN;
        parent->keys[1] = INT64_MAX;
        parent->children[0] = node;
        parent->children[1] = NULL;
        tree.root = parent;
        tree.height++;
        node->parent = parent;
#ifdef DEBUG
        printf("[Parent] Node %p.\n", parent);
#endif
      }

      rhsNode->parent = parent;
      insertToNode(parent, splitKey, rhsNode);
      node = parent;
    }
  }

  return tree;
}

int main(int argc, char *argv[]) {

  struct BPlusTree tree = initEmptyBPlusTree();

  // for (int64_t i = 0; i < 100; ++i) {
  //   insertKey(&tree, i, (void *)i);
  //   int64_t v = (int64_t)(queryKey(&tree, i));
  //   printf("Query %ld = %ld.\n", i, v);
  // }

  // for (int64_t i = 0; i < 100; ++i) {
  //   int64_t v = (int64_t)(queryKey(&tree, i));
  //   printf("Query after insert %ld = %ld.\n", i, v);
  // }

  TreeValueT *keys = malloc(sizeof(TreeValueT) * 100);
  for (int64_t i = 0; i < 100; ++i) {
    keys[i] = i;
  }
  struct BPlusTree bulkTree = bulkLoad(100, keys, (void **)keys);
  for (int64_t i = 0; i < 100; ++i) {
    int64_t v = (int64_t)(queryKey(&bulkTree, i));
    printf("Query after bulk %ld = %ld.\n", i, v);
  }

  return 0;
}