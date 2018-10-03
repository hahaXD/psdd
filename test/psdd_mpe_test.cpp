//
// Created by Yujia Shen on 10/16/17.
//
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <psdd/psdd_manager.h>
#include <psdd/psdd_node.h>
#include <psdd/psdd_unique_table.h>
#include <stack>
#include <vector>
extern "C" {
#include <sdd/sddapi.h>
}

namespace {
SddNode *GenerateCardinalitySDD(const std::vector<SddLiteral> &variables,
                                SddLiteral cardinality, SddManager *manager) {
  std::vector<std::vector<SddNode *>> cardinality_cache;
  SddNode *neg_first = sdd_manager_literal(-variables[0], manager);
  sdd_ref(neg_first, manager);
  SddNode *pos_first = sdd_manager_literal(variables[0], manager);
  sdd_ref(pos_first, manager);
  std::vector<SddNode *> first_node_cache((size_t)cardinality + 1, nullptr);
  first_node_cache[0] = neg_first;
  first_node_cache[1] = pos_first;
  cardinality_cache.push_back(first_node_cache);
  auto variable_size = variables.size();
  for (auto i = 1; i < variable_size; ++i) {
    SddLiteral cur_variable_index = variables[i];
    const auto &last_node_cache = cardinality_cache[i - 1];
    size_t upper_bound = std::min((size_t)cardinality, (size_t)i + 1);
    SddNode *pos_node = sdd_manager_literal(cur_variable_index, manager);
    sdd_ref(pos_node, manager);
    SddNode *neg_node = sdd_manager_literal(-cur_variable_index, manager);
    sdd_ref(neg_node, manager);
    std::vector<SddNode *> cur_node_cache((size_t)cardinality + 1, nullptr);
    for (auto j = 0; j <= upper_bound; ++j) {
      if (j == 0) {
        assert(last_node_cache[0] != nullptr);
        SddNode *new_node = sdd_conjoin(neg_node, last_node_cache[0], manager);
        sdd_ref(new_node, manager);
        cur_node_cache[0] = new_node;
      } else if (j == i + 1) {
        assert(last_node_cache[i] != nullptr);
        SddNode *new_node = sdd_conjoin(pos_node, last_node_cache[i], manager);
        sdd_ref(new_node, manager);
        cur_node_cache[i + 1] = new_node;
      } else {
        assert(last_node_cache[j - 1] != nullptr);
        assert(last_node_cache[j] != nullptr);
        SddNode *new_pos_node =
            sdd_conjoin(pos_node, last_node_cache[j - 1], manager);
        sdd_ref(new_pos_node, manager);
        SddNode *new_neg_node =
            sdd_conjoin(neg_node, last_node_cache[j], manager);
        sdd_ref(new_neg_node, manager);
        SddNode *new_node = sdd_disjoin(new_pos_node, new_neg_node, manager);
        sdd_ref(new_node, manager);
        sdd_deref(new_pos_node, manager);
        sdd_deref(new_neg_node, manager);
        cur_node_cache[j] = new_node;
      }
    }
    cardinality_cache.push_back(cur_node_cache);
    sdd_deref(pos_node, manager);
    sdd_deref(neg_node, manager);
  }
  SddNode *result_node = cardinality_cache[variable_size - 1][cardinality];
  sdd_ref(result_node, manager);
  for (auto i = 0; i < variable_size; ++i) {
    size_t upper_bound = std::min((size_t)cardinality, (size_t)i + 1);
    for (auto j = 0; j <= upper_bound; ++j) {
      assert(cardinality_cache[i][j] != nullptr);
      sdd_deref(cardinality_cache[i][j], manager);
    }
  }
  return result_node;
}
class PsddMPETest : public testing::Test {};
} // namespace

TEST_F(PsddMPETest, CardinalityTest) {
  Vtree *vtree = sdd_vtree_new(20, "balanced");
  SddManager *manager = sdd_manager_new(vtree);
  sdd_manager_auto_gc_and_minimize_off(manager);
  sdd_vtree_free(vtree);
  std::vector<SddLiteral> considered_vars;
  for (auto i = 1; i <= 20; ++i) {
    considered_vars.push_back(i);
  }
  SddNode *constraint_node =
      GenerateCardinalitySDD(considered_vars, 10, manager);
  PsddManager *psdd_manager =
      PsddManager::GetPsddManagerFromVtree(sdd_manager_vtree(manager));
  std::bitset<MAX_VAR> repetitive_data;
  repetitive_data.set(1);
  repetitive_data.set(3);
  repetitive_data.set(5);
  repetitive_data.set(7);
  repetitive_data.set(9);
  repetitive_data.set(11);
  repetitive_data.set(13);
  repetitive_data.set(15);
  repetitive_data.set(17);
  repetitive_data.set(19);
  PsddNode *target_node = psdd_manager->FromSdd(
      constraint_node, sdd_manager_vtree(manager), 1, psdd_manager->vtree());
  std::unordered_map<int32_t, BatchedPsddValue> examples;
  for (int32_t var = 1; var <= 20; ++var) {
    if (repetitive_data[var]) {
      BatchedPsddValue cur_values(100, true);
      examples[var] = std::move(cur_values);
    } else {
      BatchedPsddValue cur_values(100, false);
      examples[var] = std::move(cur_values);
    }
  }
  PsddNode *trained_node = psdd_manager->LearnPsddParameters(
      target_node, examples, /*data_size=*/100,
      PsddParameter::CreateFromDecimal(1), 1);
  auto result = psdd_node_util::GetMPESolution(trained_node);
  EXPECT_EQ(result.first, repetitive_data);
  delete (psdd_manager);
  sdd_manager_free(manager);
}
