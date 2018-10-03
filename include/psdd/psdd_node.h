//
// Created by Yujia Shen on 10/19/17.
//

#ifndef STRUCTURED_BAYESIAN_NETWORK_PSDD_NODE_H
#define STRUCTURED_BAYESIAN_NETWORK_PSDD_NODE_H

#include <cstdint>
#include <random>
#include <unordered_map>
#include <utility>
#include <vector>
extern "C" {
#include <sdd/sddapi.h>
};
#include <gmpxx.h>
#include <psdd/binary_data.h>
#include <psdd/psdd_parameter.h>
#include <psdd/random_double_generator.h>
#include <unordered_set>

#define LITERAL_NODE_TYPE 1
#define DECISION_NODE_TYPE 2
#define TOP_NODE_TYPE 3

using BatchedPsddValue = std::vector<bool>;
class PsddTopNode;
class PsddLiteralNode;
class PsddDecisionNode;

class PsddNode {
public:
  PsddNode(uintmax_t node_index, Vtree *vtree_node, uintmax_t flag_index);
  PsddNode(uintmax_t node_index, Vtree *vtree_node);
  virtual ~PsddNode() = default;
  virtual int node_type() const = 0; // 1 is literal node, 2 is decision node, 3
                                     // is top node with variable index
  uintmax_t node_index() const;
  uintmax_t flag_index() const;
  virtual PsddTopNode *psdd_top_node() { return nullptr; }
  virtual PsddDecisionNode *psdd_decision_node() { return nullptr; }
  virtual PsddLiteralNode *psdd_literal_node() { return nullptr; }
  virtual bool IsConsistent(const std::unordered_map<uint32_t, bool>
                                &partial_instantiation) const = 0;
  bool IsConsistent(const std::bitset<MAX_VAR> &instantiation,
                    uint32_t variable_size);
  std::size_t hash_value() const;
  Vtree *vtree_node() const;
  uintmax_t user_data() const;
  void SetUserData(uintmax_t user_data);
  bool activation_flag() const;
  void SetActivationFlag();
  void ResetActivationFlag();
  virtual void ResetDataCount() = 0;
  virtual void DirectSample(std::bitset<MAX_VAR> *instantiation,
                            RandomDoubleFromUniformGenerator *generator) = 0;
  void SetBatchedPsddValue(BatchedPsddValue value) {
    batched_psdd_value_ = std::move(value);
  }
  const BatchedPsddValue &batched_psdd_value() const {
    return batched_psdd_value_;
  }
  BatchedPsddValue *MutableBatchedPsddValue() { return &batched_psdd_value_; }
  void SetBatchedPsddContextValue(BatchedPsddValue context_value) {
    batched_psdd_context_value_ = std::move(context_value);
  }
  const BatchedPsddValue &batched_psdd_context_value() const {
    return batched_psdd_context_value_;
  }
  BatchedPsddValue *MutableBatchedPsddContextValue() {
    return &batched_psdd_context_value_;
  }

protected:
  void set_hash_value(std::size_t hash_value);

private:
  uintmax_t node_index_;
  Vtree *vtree_node_;
  uintmax_t user_data_;
  uintmax_t flag_index_;
  std::size_t hash_value_;
  bool activation_flag_;
  BatchedPsddValue batched_psdd_value_;
  BatchedPsddValue batched_psdd_context_value_;
};

class PsddLiteralNode : public PsddNode {
public:
  PsddLiteralNode(uintmax_t node_index, Vtree *vtree_node, uintmax_t flag_index,
                  int32_t literal);
  PsddLiteralNode(uintmax_t *node_index, Vtree *vtree_node,
                  uintmax_t flag_index, int32_t literal);
  PsddLiteralNode(uintmax_t *node_index, Vtree *vtree_node, int32_t literal);
  bool operator==(const PsddLiteralNode &other) const;
  ~PsddLiteralNode() override = default;
  int node_type() const override;
  PsddLiteralNode *psdd_literal_node() override { return this; }
  bool IsConsistent(const std::unordered_map<uint32_t, bool>
                        &partial_instantiation) const override;
  bool sign() const;
  uint32_t variable_index() const;
  int32_t literal() const;
  void ResetDataCount() override;
  void DirectSample(std::bitset<MAX_VAR> *instantiation,
                    RandomDoubleFromUniformGenerator *generator) override;

private:
  void CalculateHashValue();
  int32_t literal_;
};

class PsddDecisionNode : public PsddNode {
public:
  PsddDecisionNode(uintmax_t node_index, Vtree *vtree_node,
                   uintmax_t flag_index, const std::vector<PsddNode *> &primes,
                   const std::vector<PsddNode *> &subs,
                   const std::vector<PsddParameter> &parameters);
  PsddDecisionNode(uintmax_t *node_index, Vtree *vtree_node,
                   uintmax_t flag_index, const std::vector<PsddNode *> &primes,
                   const std::vector<PsddNode *> &subs,
                   const std::vector<PsddParameter> &parameters);
  PsddDecisionNode(uintmax_t *node_index, Vtree *vtree_node,
                   uintmax_t flag_index, const std::vector<PsddNode *> &primes,
                   const std::vector<PsddNode *> &subs);
  PsddDecisionNode(uintmax_t *node_index, Vtree *vtree_node,
                   const std::vector<PsddNode *> &primes,
                   const std::vector<PsddNode *> &subs);
  bool operator==(const PsddDecisionNode &other) const;
  ~PsddDecisionNode() override = default;
  int node_type() const override;
  PsddDecisionNode *psdd_decision_node() override { return this; }
  bool IsConsistent(const std::unordered_map<uint32_t, bool>
                        &partial_instantiation) const override;
  const std::vector<PsddNode *> &primes() const;
  const std::vector<PsddNode *> &subs() const;
  const std::vector<PsddParameter> &parameters() const;
  void IncrementDataCount(uintmax_t index, uintmax_t increment_size);
  void ResetDataCount() override;
  void DirectSample(std::bitset<MAX_VAR> *instantiation,
                    RandomDoubleFromUniformGenerator *generator) override;
  const std::vector<uintmax_t> &data_counts() const;

private:
  void CalculateHashValue();
  std::vector<PsddNode *> primes_;
  std::vector<PsddNode *> subs_;
  std::vector<PsddParameter> parameters_;
  std::vector<uintmax_t> data_counts_;
};

class PsddTopNode : public PsddNode {
public:
  PsddTopNode(uintmax_t node_index, Vtree *vtree_node, uintmax_t flag_index,
              uint32_t variable_index, PsddParameter true_parameter,
              PsddParameter false_parameter);
  PsddTopNode(uintmax_t *node_index, Vtree *vtree_node, uintmax_t flag_index,
              uint32_t variable_index);
  PsddTopNode(uintmax_t *node_index, Vtree *vtree_node,
              uint32_t variable_index);
  bool operator==(const PsddTopNode &other) const;
  ~PsddTopNode() override = default;
  int node_type() const override;
  PsddParameter true_parameter() const;
  PsddParameter false_parameter() const;
  PsddTopNode *psdd_top_node() override { return this; }
  bool IsConsistent(const std::unordered_map<uint32_t, bool>
                        &partial_instantiation) const override;
  uint32_t variable_index() const;
  void IncrementTrueDataCount(uintmax_t increment_size);
  void IncrementFalseDataCount(uintmax_t increment_size);
  void ResetDataCount() override;
  void DirectSample(std::bitset<MAX_VAR> *instantiation,
                    RandomDoubleFromUniformGenerator *generator) override;
  uintmax_t true_data_count() const;
  uintmax_t false_data_count() const;

private:
  void CalculateHashValue();
  uint32_t variable_index_;
  PsddParameter true_parameter_;
  PsddParameter false_parameter_;
  uintmax_t true_data_count_;
  uintmax_t false_data_count_;
};

namespace vtree_util {
std::vector<Vtree *> SerializeVtree(Vtree *root);
Vtree *CopyVtree(Vtree *root);
Vtree *
CopyVtree(Vtree *root,
          const std::unordered_map<SddLiteral, SddLiteral> &variable_map);
std::vector<SddLiteral> VariablesUnderVtree(Vtree *root);
Vtree *ProjectVtree(Vtree *orig_vtree,
                    const std::vector<SddLiteral> &variables);
Vtree *SubVtreeByVariables(Vtree *root,
                           const std::unordered_set<SddLiteral> &variables);
std::vector<SddLiteral> LeftToRightLeafTraverse(Vtree *root);
} // namespace vtree_util

namespace psdd_node_util {
std::vector<PsddNode *> SerializePsddNodes(PsddNode *root);
std::vector<PsddNode *>
SerializePsddNodes(const std::vector<PsddNode *> &root_nodes);
std::unordered_map<uintmax_t, PsddNode *>
GetCoveredPsddNodes(const std::vector<PsddNode *> &root_nodes);
void SetActivationFlag(const std::bitset<MAX_VAR> &evidence,
                       const std::vector<PsddNode *> &serialized_psdd_nodes);
std::pair<std::bitset<MAX_VAR>, Probability>
GetMPESolution(const std::vector<PsddNode *> &serialized_psdd_nodes);
std::pair<std::bitset<MAX_VAR>, Probability>
GetMPESolution(PsddNode *psdd_node);
mpz_class ModelCount(const std::vector<PsddNode *> &serialized_nodes);
Probability Evaluate(const std::bitset<MAX_VAR> &variables,
                     const std::bitset<MAX_VAR> &instantiation,
                     const std::vector<PsddNode *> &serialized_nodes);
Probability Evaluate(const std::bitset<MAX_VAR> &variables,
                     const std::bitset<MAX_VAR> &instantiation,
                     PsddNode *root_node);
bool IsConsistent(PsddNode *node, const std::bitset<MAX_VAR> &variable_mask,
                  const std::bitset<MAX_VAR> &partial_instantiation);
bool IsConsistent(const std::vector<PsddNode *> &nodes,
                  const std::bitset<MAX_VAR> &variable_mask,
                  const std::bitset<MAX_VAR> &partial_instantiation);

SddNode *ConvertPsddNodeToSddNode(
    const std::vector<PsddNode *> &serialized_psdd_nodes,
    const std::unordered_map<SddLiteral, SddLiteral> &variable_map,
    SddManager *sdd_manager);

void WritePsddToFile(PsddNode *root_node, const char *output_filename);

std::unordered_map<uint32_t, std::pair<Probability, Probability>>
GetMarginals(const std::vector<PsddNode *> &serialized_nodes);

uintmax_t GetPsddSize(PsddNode *root_node);

} // namespace psdd_node_util
#endif // STRUCTURED_BAYESIAN_NETWORK_PSDD_NODE_H
