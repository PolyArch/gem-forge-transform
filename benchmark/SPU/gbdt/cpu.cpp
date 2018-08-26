
#include <assert.h>
#include <cstdio>
#include <fstream>
#include <math.h>
#include <vector>

using namespace std;

#ifndef N
#define N 1000
#endif

#ifndef Mt
#define Mt 4
#endif

#ifndef k
#define k 64
#endif

#ifndef ratio
#define ratio 0.25
#endif

#ifndef M
#define M 4
#endif

struct instance {
  // int id;
  int f[M]; // after binning
  float y;
  float z;
};

struct SplitInfo {
  int split_feat_id;
  int thres;
  float entropy;
};

struct featInfo {
  // int hist[64];
  // float hess_hist[64];
  // float grad_hist[64];
  float label_hist[64];
  float count_hist[64];
};

struct TNode {
  // float hess;
  // float grad;
  // vector<instance> inst;
  vector<int> inst_id;
  struct TNode *child1;
  struct TNode *child2;
  // histograms associated with each node
  vector<featInfo> feat_hists;
  SplitInfo info; // check?
};

class DecisionTree {

public:
  vector<TNode> tree;
  unsigned int min_children;
  int max_depth;
  // int N; // number of instances
  // int M; // number of features
  int depth = 0;         // temp variable
  vector<instance> data; // load directly from your data

  void init_data() {
    min_children = 2;
    max_depth = 10;
    // take this as input
    // N = n;
    // M = m;
  }

  /*
  int getN(){
    return N;
  }
  int getM(){
    return M;
  }
  */

  TNode init_node() {
    // struct featInfo init_hists = {{0.0}, {0.0}, {0.0}};
    struct featInfo init_hists = {{0.0}, {0.0}};
    for (int i = 0; i < 64; ++i) {
      init_hists.label_hist[i] = 0.0;
      init_hists.count_hist[i] = 0.0;
    }
    vector<featInfo> hists;
    for (int i = 0; i < M; ++i) {
      hists.push_back(init_hists);
    }

    // vector<instance> data;
    vector<int> inst_id;
    struct SplitInfo init_info = {0, 0, 0.0};

    // TNode temp = {2.1, 3.5, data, nullptr, nullptr, hists, init_info};
    // TNode temp = {data, nullptr, nullptr, hists, init_info};
    TNode temp = {inst_id, nullptr, nullptr, hists, init_info};
    return temp;
  }

  void build_tree(struct TNode *node) {
    float entr;
    float max_entr;
    int b;
    int cur_label;
    int cur_count;
    int max_label;
    TNode child1[100];
    TNode child2[100];

    vector<TNode *> cur_nodes;
    cur_nodes.push_back(node);
    unsigned node_id = 0;
    // for(unsigned node_id=0; node_id<cur_nodes.size() && depth < max_depth;
    // ++node_id){
    {
      max_entr = 0.0;
      node = cur_nodes[node_id];
      child1[node_id] = init_node();
      child2[node_id] = init_node();
      node->child1 = &child1[node_id];
      node->child2 = &child2[node_id];
      // node->child1->inst_id.resize(node->inst_id.size());
      // node->child2->inst_id.resize(node->inst_id.size());
      // can parallelize across features
      for (int j = 1; j < Mt; ++j) {

        // histogram building
        for (unsigned int i = 0; i < node->inst_id.size(); ++i) {
          b = data[node->inst_id[i]].f[j];
          node->feat_hists[j].label_hist[b] += data[node->inst_id[i]].y;
          node->feat_hists[j].count_hist[b] += data[node->inst_id[i]].z;
          // node->feat_hists[j].count_hist[b] += 1;
        }

        // find cumulative sum
        for (int i = 1; i < 64; ++i) {
          node->feat_hists[j].label_hist[i] +=
              node->feat_hists[j].label_hist[i - 1];
          node->feat_hists[j].count_hist[i] +=
              node->feat_hists[j].label_hist[i - 1];
        }
        // reduction step
        max_label = node->feat_hists[j].label_hist[63];
        for (unsigned int thresh = 0; thresh < 64;
             ++thresh) { // threshold of 0 and 63 are not allowed
          cur_label = node->feat_hists[j].label_hist[thresh];
          cur_count = node->feat_hists[j].count_hist[thresh];
          entr = pow(cur_label, 2) / (cur_count) +
                 pow(max_label - cur_label, 2) / (N - cur_count);
          if (entr > max_entr) {
            max_entr = entr;
            node->info.split_feat_id = j;
            node->info.thres = thresh;
          }
        }
      }

      for (unsigned int i = 0; i < node->inst_id.size(); ++i) {
        if (data[node->inst_id[i]].f[node->info.split_feat_id] <=
            node->info.thres) {
          // allot to child1
          node->child1->inst_id.push_back(i);
        } else {
          node->child2->inst_id.push_back(i);
        }
      }
      // cout << "Depth: " << depth << "\n";
      if (node->child1->inst_id.size() > min_children) {
        cur_nodes.push_back(node->child1);
        // cout << "child1 size: " << node->child1->inst_id.size() <<  "\n";
      }
      if (node->child2->inst_id.size() > min_children) {
        cur_nodes.push_back(node->child2);
        // cout << "child2 size: " << node->child2->inst_id.size() <<  "\n";
      }
      if (node->child1->inst_id.size() > min_children ||
          node->child2->inst_id.size() > min_children) {
        depth++;
      }
    }
  }
};

int main() {
  DecisionTree tree;

  // tree.init_data(65536,8);
  tree.init_data();

  // N = 3; M = 2;
  float output1;
  float output2;
  int id = 0;
  // instance temp = {1, binned_input, 100};
  struct instance temp;
  for (int i = 0; i < M; ++i) {
    temp.f[i] = 0;
  }
  temp.y = 100.0f;
  temp.z = 100.0f;
  FILE *train_file = fopen("input.data", "r");
  if (!train_file) {
    printf("Error opening file\n");
  }

  printf("Started reading file!\n");

  while (1) {
    int done = 0;
    for (int i = 0; i < M; i++) {
      if (fscanf(train_file, "%d", &temp.f[i]) != 1) {
        done = 1;
        break;
      }
    }
    if (done == 1) {
      break;
    }
    fscanf(train_file, "%f %f", &output1, &output2);

    tree.data.push_back(temp);
    id++;
  }

  fclose(train_file);
  printf("Done reading file!\n");

  // struct featInfo init_hists = {{0.0}, {0.0}, {0.0}};
  struct featInfo init_hists = {{0.0}, {0.0}};
  for (int i = 0; i < 64; ++i) {
    init_hists.label_hist[i] = 0.0;
    init_hists.count_hist[i] = 0.0;
  }
  vector<featInfo> hists;
  for (int i = 0; i < M; ++i) {
    hists.push_back(init_hists);
  }
  vector<int> inst_id;
  /*
  for(unsigned i=0; i<tree.data.size(); ++i) {
    inst_id.push_back(i);
  }
  */

  FILE *id_file = fopen("inst_id.data", "r");

  printf("Started reading inst id file!\n");
  int l = 0;

  while (fscanf(id_file, "%d", &l) == 1) {
    inst_id.push_back(l);
  }

  struct SplitInfo init_info = {0, 0, 0.0};
  // struct TNode node = {2.3, 3.5, data, nullptr, nullptr, hists, init_info};
  struct TNode node = {inst_id, nullptr, nullptr, hists, init_info};
  struct TNode *root = &node;

  tree.build_tree(root);

  return 0;
}