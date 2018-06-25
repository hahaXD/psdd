//
// Created by jason on 2/28/18.
//

#ifndef STRUCTURED_BAYESIAN_NETWORK_CNF_H
#define STRUCTURED_BAYESIAN_NETWORK_CNF_H

#include <vector>
#include <psdd/psdd_node.h>
#include <psdd/psdd_manager.h>
class CNF {
 public:
  CNF() = default;
  explicit CNF(const char *filename);
  explicit CNF(const std::vector<std::vector<SddLiteral>> &clauses);
  const std::vector<std::vector<SddLiteral>> &clauses() const;
  /*
  PsddNode *CompileToSddWithEvidence(const std::unordered_map<uint32_t, bool> &evid, Vtree *vtree) const;
  bool CheckConstraintWithPartialInstantiation(const std::bitset<MAX_VAR> &variable_mask,
                                               const std::bitset<MAX_VAR> &variable_instantiation) const;
  */
  PsddNode* Compile(PsddManager* psdd_manager, uintmax_t flag_index) const;
 private:
  std::vector<std::vector<SddLiteral>> clauses_;
};

#endif //STRUCTURED_BAYESIAN_NETWORK_CNF_H
