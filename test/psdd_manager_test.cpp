//
// Created by Yujia Shen on 4/5/18.
//
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <unordered_map>
#include <psdd_manager.h>
extern "C" {
#include "sddapi.h"
}

namespace {
SddNode *CardinalityK(uint32_t variable_size_usign,
                      uint32_t k,
                      SddManager *manager,
                      std::unordered_map<uint32_t, std::unordered_map<uint32_t, SddNode *>> *cache) {
  auto variable_size = (SddLiteral) variable_size_usign;
  auto cache_it = cache->find(variable_size_usign);
  if (cache_it != cache->end()) {
    auto second_cache_it = cache_it->second.find(k);
    if (second_cache_it != cache_it->second.end()) {
      return second_cache_it->second;
    }
  } else {
    cache->insert({variable_size, {}});
    cache_it = cache->find(variable_size_usign);
  }
  if (variable_size == 1) {
    if (k == 0) {
      SddNode *result = sdd_manager_literal(-1, manager);
      cache_it->second.insert({0, result});
      return result;
    } else {
      SddNode *result = sdd_manager_literal(1, manager);
      cache_it->second.insert({1, result});
      return result;
    }
  } else {
    if (k == 0) {
      SddNode *remaining = CardinalityK(variable_size_usign - 1, 0, manager, cache);
      SddNode *result = sdd_conjoin(sdd_manager_literal(-variable_size, manager), remaining, manager);
      cache_it->second.insert({0, result});
      return result;
    } else if (k == variable_size) {
      SddNode *remaining = CardinalityK(variable_size_usign - 1, k - 1, manager, cache);
      SddNode *result = sdd_conjoin(sdd_manager_literal(variable_size, manager), remaining, manager);
      cache_it->second.insert({k, result});
      return result;
    } else {
      SddNode *remaining_positive = CardinalityK(variable_size_usign - 1, k - 1, manager, cache);
      SddNode *remaining_negative = CardinalityK(variable_size_usign - 1, k, manager, cache);
      SddNode *positive_case = sdd_conjoin(sdd_manager_literal(variable_size, manager), remaining_positive, manager);
      SddNode *negative_case = sdd_conjoin(sdd_manager_literal(-variable_size, manager), remaining_negative, manager);
      SddNode *result = sdd_disjoin(positive_case, negative_case, manager);
      cache_it->second.insert({k, result});
      return result;
    }
  }
}

}

TEST(SDD_TO_PSDD_TEST, MODEL_COUNT_TEST) {
  Vtree *vtree = sdd_vtree_new(10, "right");
  SddManager *sdd_manager = sdd_manager_new(vtree);
  sdd_vtree_free(vtree);
  sdd_manager_auto_gc_and_minimize_off(sdd_manager);
  std::unordered_map<uint32_t, std::unordered_map<uint32_t, SddNode *>> cache;
  SddNode *cardinality_node = CardinalityK(10, 5, sdd_manager, &cache);
  std::unordered_map<uint32_t, uint32_t> variable_mapping;
  for (auto i = 0; i < 10; ++i) {
    variable_mapping[(uint32_t) i + 1] = (uint32_t) i + 1;
  }
  PsddManager *psdd_manager = PsddManager::GetPsddManagerFromSddVtree(sdd_manager_vtree(sdd_manager), variable_mapping);
  PsddNode *result_psdd =
      psdd_manager->ConvertSddToPsdd(cardinality_node, sdd_manager_vtree(sdd_manager), 0, variable_mapping);
  std::vector<PsddNode *> serialized_nodes = psdd_node_util::SerializePsddNodes(result_psdd);
  EXPECT_EQ(sdd_global_model_count(cardinality_node, sdd_manager), psdd_node_util::ModelCount(serialized_nodes));
  delete psdd_manager;
  sdd_manager_free(sdd_manager);
}

TEST(SDD_TO_PSDD_TEST, VARIABLE_MAP_TEST) {
  Vtree *vtree = sdd_vtree_new(10, "right");
  SddManager *sdd_manager = sdd_manager_new(vtree);
  sdd_vtree_free(vtree);
  sdd_manager_auto_gc_and_minimize_off(sdd_manager);
  std::unordered_map<uint32_t, std::unordered_map<uint32_t, SddNode *>> cache;
  SddNode *first_true = sdd_manager_literal(1, sdd_manager);
  std::unordered_map<uint32_t, uint32_t> variable_mapping;
  for (auto i = 0; i < 10; ++i) {
    variable_mapping[(uint32_t) i + 1] = (uint32_t) i + 11;
  }
  PsddManager *psdd_manager = PsddManager::GetPsddManagerFromSddVtree(sdd_manager_vtree(sdd_manager), variable_mapping);
  PsddNode
      *result_psdd = psdd_manager->ConvertSddToPsdd(first_true, sdd_manager_vtree(sdd_manager), 0, variable_mapping);
  auto serialized_psdd = psdd_node_util::SerializePsddNodes(result_psdd);
  {
    std::bitset<MAX_VAR> variable_mask;
    variable_mask.set(11);
    std::bitset<MAX_VAR> variable_instantiation;
    EXPECT_TRUE(!psdd_node_util::IsConsistent(serialized_psdd, variable_mask, variable_instantiation));
    EXPECT_TRUE(psdd_node_util::IsConsistent(serialized_psdd, variable_mask, variable_mask));
  }
  for (auto i = 12; i <= 20; ++i) {
    std::bitset<MAX_VAR> variable_instantiation;
    std::bitset<MAX_VAR> variable_mask;
    variable_mask.set((uint32_t) i);
    EXPECT_TRUE(psdd_node_util::IsConsistent(serialized_psdd, variable_mask, variable_instantiation));
    EXPECT_TRUE(psdd_node_util::IsConsistent(serialized_psdd, variable_mask, variable_mask));
  }
  delete psdd_manager;
  sdd_manager_free(sdd_manager);
}

TEST(PSDD_MANAGER_TEST, GET_TOP_NODE) {
  Vtree *vtree = sdd_vtree_new(10, "right");
  PsddManager *psdd_manager = PsddManager::GetPsddManagerFromVtree(vtree);
  sdd_vtree_free(vtree);
  vtree = psdd_manager->vtree();
  auto vtree_nodes = vtree_util::SerializeVtree(vtree);
  PsddTopNode *new_top_node = psdd_manager->GetPsddTopNode(1,
                                                           0,
                                                           PsddParameter::CreateFromDecimal(0.1),
                                                           PsddParameter::CreateFromDecimal(0.9));
  EXPECT_EQ(new_top_node->true_parameter(), PsddParameter::CreateFromDecimal(0.1));
  EXPECT_EQ(new_top_node->false_parameter(), PsddParameter::CreateFromDecimal(0.9));
  PsddTopNode *second_new_top_node = psdd_manager->GetPsddTopNode(1,
                                                                  0,
                                                                  PsddParameter::CreateFromDecimal(0.1),
                                                                  PsddParameter::CreateFromDecimal(0.9));
  EXPECT_EQ(second_new_top_node, new_top_node);
  second_new_top_node = psdd_manager->GetPsddTopNode(1,
                                                     0,
                                                     PsddParameter::CreateFromDecimal(0.9),
                                                     PsddParameter::CreateFromDecimal(0.1));
  EXPECT_NE(second_new_top_node, new_top_node);
  EXPECT_EQ(second_new_top_node->true_parameter(), PsddParameter::CreateFromDecimal(0.9));
  EXPECT_EQ(second_new_top_node->false_parameter(), PsddParameter::CreateFromDecimal(0.1));
  second_new_top_node = psdd_manager->GetPsddTopNode(1,
                                                     1,
                                                     PsddParameter::CreateFromDecimal(0.1),
                                                     PsddParameter::CreateFromDecimal(0.9));
  EXPECT_NE(second_new_top_node, new_top_node);
  delete (psdd_manager);
}

TEST(PSDD_MANAGER_TEST, GET_LIERAL_NODE) {
  Vtree *vtree = sdd_vtree_new(10, "right");
  PsddManager *psdd_manager = PsddManager::GetPsddManagerFromVtree(vtree);
  sdd_vtree_free(vtree);
  vtree = psdd_manager->vtree();
  auto vtree_nodes = vtree_util::SerializeVtree(vtree);
  PsddLiteralNode *new_literal_node = psdd_manager->GetPsddLiteralNode(-1, 0);
  EXPECT_EQ(false, new_literal_node->sign());
  EXPECT_EQ(-1, new_literal_node->literal());
  PsddLiteralNode *second_new_literal_node = psdd_manager->GetPsddLiteralNode(-1, 0);
  EXPECT_EQ(new_literal_node, second_new_literal_node);
  second_new_literal_node = psdd_manager->GetPsddLiteralNode(1, 0);
  EXPECT_NE(new_literal_node, second_new_literal_node);
  EXPECT_EQ(1, second_new_literal_node->literal());
  second_new_literal_node = psdd_manager->GetPsddLiteralNode(-1, 1);
  EXPECT_EQ(-1, second_new_literal_node->literal());
  EXPECT_NE(second_new_literal_node, new_literal_node);
  delete (psdd_manager);
}

TEST(PSDD_MANAGER_TEST, GET_CONFORMED_DECISION_NODE) {
  Vtree *vtree = sdd_vtree_new(10, "right");
  PsddManager *psdd_manager = PsddManager::GetPsddManagerFromVtree(vtree);
  sdd_vtree_free(vtree);
  vtree = psdd_manager->vtree();
  auto vtree_nodes = vtree_util::SerializeVtree(vtree);
  PsddLiteralNode *neg_one = psdd_manager->GetPsddLiteralNode(-1, 0);
  PsddTopNode *top_ten = psdd_manager->GetPsddTopNode(10,
                                                      0,
                                                      PsddParameter::CreateFromDecimal(0.9),
                                                      PsddParameter::CreateFromDecimal(0.1));
  PsddLiteralNode *pos_one = psdd_manager->GetPsddLiteralNode(1, 0);
  PsddTopNode *top_ten_second = psdd_manager->GetPsddTopNode(10,
                                                             0,
                                                             PsddParameter::CreateFromDecimal(0.1),
                                                             PsddParameter::CreateFromDecimal(0.9));
  PsddDecisionNode *new_decn_node = psdd_manager->GetConformedPsddDecisionNode({pos_one, neg_one},
                                                                               {top_ten, top_ten_second},
                                                                               {PsddParameter::CreateFromDecimal(0.8),
                                                                                PsddParameter::CreateFromDecimal(0.2)},
                                                                               0);
  PsddDecisionNode *new_second_node = psdd_manager->GetConformedPsddDecisionNode({neg_one, pos_one},
                                                                                 {top_ten_second, top_ten},
                                                                                 {PsddParameter::CreateFromDecimal(0.2),
                                                                                  PsddParameter::CreateFromDecimal(0.8)},
                                                                                 0);
  std::bitset<MAX_VAR> variables;
  std::bitset<MAX_VAR> instantiation_one;
  std::bitset<MAX_VAR> instantiation_two;
  for (auto i = 1; i <= 10; ++i) {
    variables.set((size_t)i);
    instantiation_one.set((size_t)i);
    if (i == 1 || i == 10) {
      instantiation_two.set((size_t)i);
    }
  }
  Probability pr = psdd_node_util::Evaluate(variables, instantiation_one, new_decn_node);
  Probability pr2 = psdd_node_util::Evaluate(variables, instantiation_two, new_decn_node);
  EXPECT_EQ(pr, pr2);
  EXPECT_EQ(new_decn_node, new_second_node);
  EXPECT_EQ(pr, PsddParameter::CreateFromDecimal(0.0028125));
  new_second_node = psdd_manager->GetConformedPsddDecisionNode({neg_one, pos_one},
                                                               {top_ten, top_ten_second},
                                                               {PsddParameter::CreateFromDecimal(0.2),
                                                                PsddParameter::CreateFromDecimal(0.8)},
                                                               0);
  EXPECT_NE(new_second_node, new_decn_node);
  new_second_node = psdd_manager->GetConformedPsddDecisionNode({pos_one, neg_one},
                                                               {top_ten, top_ten_second},
                                                               {PsddParameter::CreateFromDecimal(0.8),
                                                                PsddParameter::CreateFromDecimal(0.2)},
                                                               1);
  EXPECT_NE(new_decn_node, new_second_node);
  delete (psdd_manager);
}

TEST(PSDD_MANAGER_TEST, LOAD_PSDD_TEST){

}