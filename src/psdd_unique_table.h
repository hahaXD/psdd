//
// Created by Yujia Shen on 10/20/17.
//

#ifndef STRUCTURED_BAYESIAN_NETWORK_PSDD_UNIQUE_TABLE_H
#define STRUCTURED_BAYESIAN_NETWORK_PSDD_UNIQUE_TABLE_H
#include <vector>
#include <unordered_set>
extern "C" {
#include <sdd/sddapi.h>
};
class PsddNode;

class PsddUniqueTable {
 public:
  virtual ~PsddUniqueTable() = default;
  virtual PsddNode *GetUniqueNode(PsddNode *node, uintmax_t *node_index)= 0;
  virtual void DeletePsddNodesWithoutFlagIndexes(const std::unordered_set<uintmax_t> &flag_index) = 0;
  virtual void DeleteUnusedPsddNodes(const std::vector<PsddNode *> &used_psdd_nodes) = 0;
  static PsddUniqueTable *GetPsddUniqueTable();
};

#endif //STRUCTURED_BAYESIAN_NETWORK_PSDD_UNIQUE_TABLE_H
