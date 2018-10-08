//
// Created by Yujia Shen on 10/3/18.
//
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cmath>
#include <psdd/psdd_manager.h>
#include <psdd/psdd_node.h>
#include <psdd/psdd_unique_table.h>
#include <stack>
#include <vector>
extern "C" {
#include <sdd/sddapi.h>
}

namespace {
SddNode *GenerateParitySDD(SddManager *sdd_manager) {
  std::vector<std::pair<SddNode *, SddNode *>> nodes(
      sdd_manager_var_count(sdd_manager) + 1);
  for (SddLiteral i = 1; i <= sdd_manager_var_count(sdd_manager); ++i) {
    if (i == 1) {
      nodes[1] = std::make_pair(sdd_manager_literal(-1, sdd_manager),
                                sdd_manager_literal(1, sdd_manager));
      continue;
    }
    SddNode *pos_literal = sdd_manager_literal(i, sdd_manager);
    SddNode *neg_literal = sdd_manager_literal(-i, sdd_manager);
    SddNode *even_case = sdd_disjoin(
        sdd_conjoin(pos_literal, nodes[i - 1].second, sdd_manager),
        sdd_conjoin(neg_literal, nodes[i - 1].first, sdd_manager), sdd_manager);
    SddNode *odd_case =
        sdd_disjoin(sdd_conjoin(pos_literal, nodes[i - 1].first, sdd_manager),
                    sdd_conjoin(neg_literal, nodes[i - 1].second, sdd_manager),
                    sdd_manager);
    nodes[i] = std::make_pair(even_case, odd_case);
  }
  return nodes[sdd_manager_var_count(sdd_manager)].first;
}
} // namespace

TEST(PsddLearningTest, LearningWithZeroData) {
  Vtree *vtree = sdd_vtree_new(4, "right");
  SddManager *sdd_manager = sdd_manager_new(vtree);
  sdd_manager_auto_gc_and_minimize_off(sdd_manager);
  SddNode *parity_node = GenerateParitySDD(sdd_manager);
  PsddManager *psdd_manager = PsddManager::GetPsddManagerFromVtree(vtree);
  sdd_vtree_free(vtree);
  PsddNode *parity_psdd = psdd_manager->FromSdd(
      parity_node, sdd_manager_vtree(sdd_manager), 1, psdd_manager->vtree());
  std::unordered_map<int32_t, std::vector<bool>> examples;
  PsddNode *learned_parity_psdd = psdd_manager->LearnPsddParameters(
      parity_psdd, examples, /*data_size=*/0,
      PsddParameter::CreateFromDecimal(1), /*flag_index=*/1);
  std::bitset<MAX_VAR> variables;
  std::bitset<MAX_VAR> instantiation;
  variables.set();
  // 0 0 0 0
  instantiation.reset();
  auto result =
      psdd_node_util::Evaluate(variables, instantiation, learned_parity_psdd);
  auto expected_result = Probability::CreateFromDecimal(1.0 / 8);
  EXPECT_TRUE(std::abs((result / expected_result).parameter()) <= 0.000001);
  // 0 0 1 1
  instantiation.reset();
  instantiation.set(1);
  instantiation.set(2);
  result =
      psdd_node_util::Evaluate(variables, instantiation, learned_parity_psdd);
  expected_result = Probability::CreateFromDecimal(1.0 / 8);
  EXPECT_TRUE(std::abs((result / expected_result).parameter()) <= 0.000001);
  // 0 1 0 1
  instantiation.reset();
  instantiation.set(1);
  instantiation.set(3);
  result =
      psdd_node_util::Evaluate(variables, instantiation, learned_parity_psdd);
  expected_result = Probability::CreateFromDecimal(1.0 / 8);
  EXPECT_TRUE(std::abs((result / expected_result).parameter()) <= 0.000001);
  // 0 1 1 0
  instantiation.reset();
  instantiation.set(2);
  instantiation.set(3);
  result =
      psdd_node_util::Evaluate(variables, instantiation, learned_parity_psdd);
  expected_result = Probability::CreateFromDecimal(1.0 / 8);
  EXPECT_TRUE(std::abs((result / expected_result).parameter()) <= 0.000001);
  // 1 0 0 1
  instantiation.reset();
  instantiation.set(1);
  instantiation.set(4);
  result =
      psdd_node_util::Evaluate(variables, instantiation, learned_parity_psdd);
  expected_result = Probability::CreateFromDecimal(1.0 / 8);
  EXPECT_TRUE(std::abs((result / expected_result).parameter()) <= 0.000001);
  // 1 0 1 0
  instantiation.reset();
  instantiation.set(2);
  instantiation.set(4);
  result =
      psdd_node_util::Evaluate(variables, instantiation, learned_parity_psdd);
  expected_result = Probability::CreateFromDecimal(1.0 / 8);
  EXPECT_TRUE(std::abs((result / expected_result).parameter()) <= 0.000001);
  // 1 1 0 0
  instantiation.reset();
  instantiation.set(3);
  instantiation.set(4);
  result =
      psdd_node_util::Evaluate(variables, instantiation, learned_parity_psdd);
  expected_result = Probability::CreateFromDecimal(1.0 / 8);
  EXPECT_TRUE(std::abs((result / expected_result).parameter()) <= 0.000001);
  // 1 1 1 1
  instantiation.set();
  result =
      psdd_node_util::Evaluate(variables, instantiation, learned_parity_psdd);
  expected_result = Probability::CreateFromDecimal(1.0 / 8);
  EXPECT_TRUE(std::abs((result / expected_result).parameter()) <= 0.000001);
  delete (psdd_manager);
  sdd_manager_free(sdd_manager);
}

TEST(PsddLearningTest, ParityLearningExample) {
  Vtree *vtree = sdd_vtree_new(4, "right");
  SddManager *sdd_manager = sdd_manager_new(vtree);
  sdd_manager_auto_gc_and_minimize_off(sdd_manager);
  SddNode *parity_node = GenerateParitySDD(sdd_manager);
  PsddManager *psdd_manager = PsddManager::GetPsddManagerFromVtree(vtree);
  sdd_vtree_free(vtree);
  PsddNode *parity_psdd = psdd_manager->FromSdd(
      parity_node, sdd_manager_vtree(sdd_manager), 1, psdd_manager->vtree());
  std::unordered_map<int32_t, BatchedPsddValue> examples;
  examples[1] = BatchedPsddValue(1, true);
  examples[2] = BatchedPsddValue(1, true);
  examples[3] = BatchedPsddValue(1, true);
  examples[4] = BatchedPsddValue(1, true);
  PsddNode *learned_parity_psdd = psdd_manager->LearnPsddParameters(
      parity_psdd, examples, 1, PsddParameter::CreateFromDecimal(1), 1);
  std::bitset<MAX_VAR> variables;
  std::bitset<MAX_VAR> instantiation;
  variables.set();
  // 0 0 0 0
  instantiation.reset();
  auto result =
      psdd_node_util::Evaluate(variables, instantiation, learned_parity_psdd);
  auto expected_result = Probability::CreateFromDecimal(1.0 / 18);
  EXPECT_TRUE(std::abs((result / expected_result).parameter()) <= 0.000001);
  // 0 0 1 1
  instantiation.reset();
  instantiation.set(1);
  instantiation.set(2);
  result =
      psdd_node_util::Evaluate(variables, instantiation, learned_parity_psdd);
  expected_result = Probability::CreateFromDecimal(4.0 / 27);
  EXPECT_TRUE(std::abs((result / expected_result).parameter()) <= 0.000001);
  // 0 1 0 1
  instantiation.reset();
  instantiation.set(1);
  instantiation.set(3);
  result =
      psdd_node_util::Evaluate(variables, instantiation, learned_parity_psdd);
  expected_result = Probability::CreateFromDecimal(1.0 / 9);
  EXPECT_TRUE(std::abs((result / expected_result).parameter()) <= 0.000001);
  // 0 1 1 0
  instantiation.reset();
  instantiation.set(2);
  instantiation.set(3);
  result =
      psdd_node_util::Evaluate(variables, instantiation, learned_parity_psdd);
  expected_result = Probability::CreateFromDecimal(1.0 / 12);
  EXPECT_TRUE(std::abs((result / expected_result).parameter()) <= 0.000001);
  // 1 0 0 1
  instantiation.reset();
  instantiation.set(1);
  instantiation.set(4);
  result =
      psdd_node_util::Evaluate(variables, instantiation, learned_parity_psdd);
  expected_result = Probability::CreateFromDecimal(1.0 / 9);
  EXPECT_TRUE(std::abs((result / expected_result).parameter()) <= 0.000001);
  // 1 0 1 0
  instantiation.reset();
  instantiation.set(2);
  instantiation.set(4);
  result =
      psdd_node_util::Evaluate(variables, instantiation, learned_parity_psdd);
  expected_result = Probability::CreateFromDecimal(1.0 / 12);
  EXPECT_TRUE(std::abs((result / expected_result).parameter()) <= 0.000001);
  // 1 1 0 0
  instantiation.reset();
  instantiation.set(3);
  instantiation.set(4);
  result =
      psdd_node_util::Evaluate(variables, instantiation, learned_parity_psdd);
  expected_result = Probability::CreateFromDecimal(1.0 / 9);
  EXPECT_TRUE(std::abs((result / expected_result).parameter()) <= 0.000001);
  // 1 1 1 1
  instantiation.set();
  result =
      psdd_node_util::Evaluate(variables, instantiation, learned_parity_psdd);
  expected_result = Probability::CreateFromDecimal(8.0 / 27);
  EXPECT_TRUE(std::abs((result / expected_result).parameter()) <= 0.000001);

  delete (psdd_manager);
  sdd_manager_free(sdd_manager);
}
