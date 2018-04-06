//
// Created by Yujia Shen on 3/20/18.
//

#ifndef PSDD_PSDD_MANAGER_H
#define PSDD_PSDD_MANAGER_H
#include "psdd_node.h"
extern "C" {
#include <sddapi.h>
};

class PsddManager {
 public:
  static PsddManager *GetPsddManager(Vtree * sdd_vtree, const std::unordered_map<uint32_t, uint32_t>& variable_mapping);
  virtual ~PsddManager() = default;
  virtual void DeleteUnusedPsddNodes(const std::vector<PsddNode *> &used_nodes) = 0;
  virtual PsddNode *ConvertSddToPsdd(SddNode *root_node,
                                     Vtree *sdd_vtree,
                                     uintmax_t flag_index,
                                     const std::unordered_map<uint32_t, uint32_t> &variable_mapping) = 0;

};

#endif //PSDD_PSDD_MANAGER_H
