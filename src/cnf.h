//
// Created by jason on 2/28/18.
//

#ifndef STRUCTURED_BAYESIAN_NETWORK_CNF_H
#define STRUCTURED_BAYESIAN_NETWORK_CNF_H

#include <vector>
#include "psdd_node.h"
class CNF {
 public:
  CNF() = default;
  explicit CNF(const char *filename);
  explicit CNF(const std::vector<std::vector<int>> &clauses);
  const std::vector<std::vector<int>> &clauses() const;
  PsddNode *CompileToSddWithEvidence(const std::unordered_map<uint32_t, bool> &evid, Vtree *vtree) const;
  bool CheckConstraintWithPartialInstantiation(const std::bitset<MAX_VAR> &variable_mask,
                                               const std::bitset<MAX_VAR> &variable_instantiation) const;
 private:
  std::vector<std::vector<int>> clauses_;
};

#endif //STRUCTURED_BAYESIAN_NETWORK_CNF_H
