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
  EXPECT_EQ(sdd_global_model_count(cardinality_node, sdd_manager),
            psdd_node_util::ModelCount(serialized_nodes).get_ui());
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
    variables.set((size_t) i);
    instantiation_one.set((size_t) i);
    if (i == 1 || i == 10) {
      instantiation_two.set((size_t) i);
    }
  }
  Probability pr = psdd_node_util::Evaluate(variables, instantiation_one, new_decn_node);
  Probability pr2 = psdd_node_util::Evaluate(variables, instantiation_two, new_decn_node);
  EXPECT_EQ(pr, pr2);
  EXPECT_EQ(new_decn_node, new_second_node);
  EXPECT_EQ(pr, PsddParameter::CreateFromDecimal(0.72));
  EXPECT_EQ(psdd_node_util::ModelCount(psdd_node_util::SerializePsddNodes(new_decn_node)).get_ui(), 1 << 10);
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

TEST(PSDD_MANAGER_TEST, LOAD_PSDD_TEST) {
  Vtree *v = sdd_vtree_new(8, "balanced");
  std::unordered_map<uint32_t, uint32_t> variable_identical_map;
  for (auto i = 1; i <= 8; ++i) {
    variable_identical_map[(uint32_t) i] = (uint32_t) i;
  }
  SddManager *test_sdd_manager = sdd_manager_new(v);
  sdd_manager_auto_gc_and_minimize_off(test_sdd_manager);
  PsddManager *test_psdd_manager = PsddManager::GetPsddManagerFromSddVtree(v, variable_identical_map);
  sdd_vtree_free(v);
  std::unordered_map<uint32_t, std::unordered_map<uint32_t, SddNode *>> cache;
  SddNode *card_node = CardinalityK(8, 4, test_sdd_manager, &cache);
  PsddNode *card_psdd =
      test_psdd_manager->ConvertSddToPsdd(card_node, sdd_manager_vtree(test_sdd_manager), 0, variable_identical_map);
  PsddNode *second_card_psdd = test_psdd_manager->LoadPsddNode(test_psdd_manager->vtree(), card_psdd, 0);
  EXPECT_EQ(second_card_psdd, card_psdd);
  second_card_psdd = test_psdd_manager->LoadPsddNode(test_psdd_manager->vtree(), card_psdd, 1);
  EXPECT_NE(second_card_psdd, card_psdd);
  size_t bound = 1 << 9;
  std::bitset<MAX_VAR> variable_mask = bound - 1;
  std::vector<PsddNode *> serialized_card_psdd = psdd_node_util::SerializePsddNodes(card_psdd);
  std::vector<PsddNode *> serialized_second_card_psdd = psdd_node_util::SerializePsddNodes(second_card_psdd);
  for (auto i = 0; i < bound; ++i) {
    std::bitset<MAX_VAR> cur_instantiation = i;
    EXPECT_EQ(psdd_node_util::IsConsistent(serialized_card_psdd, variable_mask, cur_instantiation),
              psdd_node_util::IsConsistent(serialized_second_card_psdd, variable_mask, cur_instantiation));
  }
  // Check with a different vtree
  Vtree *second_v = sdd_vtree_new(16, "balanced");
  std::unordered_map<uint32_t, uint32_t> variable_map;
  for (auto i = 1; i <= 16; ++i) {
    uint32_t new_index = 0;
    if (i % 2 == 0) {
      new_index = (uint32_t) i / 2;
    } else {
      new_index = (uint32_t) (i + 1) / 2 + 5;
    }
    variable_map[(uint32_t) i] = new_index;
  }
  PsddManager *different_psdd_manager = PsddManager::GetPsddManagerFromSddVtree(second_v, variable_map);
  PsddNode *second_psdd = different_psdd_manager->LoadPsddNode(different_psdd_manager->vtree(), card_psdd, 0);
  sdd_vtree_free(second_v);
  std::vector<PsddNode *> serialized_second_psdd = psdd_node_util::SerializePsddNodes(second_psdd);
  for (auto i = 1; i <= 16; ++i) {
    std::bitset<MAX_VAR> cur_instantiation = i;
    EXPECT_EQ(psdd_node_util::IsConsistent(serialized_card_psdd, variable_mask, cur_instantiation),
              psdd_node_util::IsConsistent(serialized_second_psdd, variable_mask, cur_instantiation));
  }
  auto second_node_mc = psdd_node_util::ModelCount(serialized_second_psdd).get_ui();
  auto card_psdd_mc = psdd_node_util::ModelCount(serialized_card_psdd).get_ui();
  EXPECT_EQ(card_psdd_mc << 8, second_node_mc);
  delete (test_psdd_manager);
  delete (different_psdd_manager);
  sdd_manager_free(test_sdd_manager);
}

TEST(PSDD_MANAGER_TEST, MULTIPLY_TEST) {
  Vtree *vtree = sdd_vtree_new(16, "balanced");
  PsddManager *manager = PsddManager::GetPsddManagerFromVtree(vtree);
  sdd_vtree_free(vtree);
  PsddNode *uniform_true_node = manager->GetTrueNode(manager->vtree(), 0);
  auto result = manager->Multiply(uniform_true_node, uniform_true_node, 0);
  size_t cap = 1 << 17;
  std::bitset<MAX_VAR> variables = cap - 1;
  std::vector<PsddNode *> serialized_psdd_nodes = psdd_node_util::SerializePsddNodes(result.first);
  for (auto i = 0; i < cap; ++i) {
    std::bitset<MAX_VAR> cur_instantiation = i;
    EXPECT_EQ(psdd_node_util::Evaluate(variables, cur_instantiation, serialized_psdd_nodes),
              PsddParameter::CreateFromDecimal(pow(2, -16)));
  }
  EXPECT_EQ(result.second, PsddParameter::CreateFromDecimal(pow(2, 16)));
  PsddNode *one_true = manager->NormalizePsddNode(manager->vtree(), manager->GetPsddLiteralNode(1, 0), 0);
  PsddNode *one_false = manager->NormalizePsddNode(manager->vtree(), manager->GetPsddLiteralNode(-1, 0), 0);
  result = manager->Multiply(one_true, one_false, 0);
  EXPECT_EQ(result.first, nullptr);
  EXPECT_EQ(result.second, PsddParameter::CreateFromDecimal(0));
  delete (manager);
}

TEST(PSDD_MANAGER_TEST, MULTIPLY_TEST2) {
  RandomDoubleFromGammaGenerator generator(1, 1, 0);
  Vtree *vtree = sdd_vtree_new(8, "right");
  PsddManager *manager = PsddManager::GetPsddManagerFromVtree(vtree);
  SddManager *sdd_manager = sdd_manager_new(vtree);
  sdd_vtree_free(vtree);
  std::unordered_map<uint32_t, std::unordered_map<uint32_t, SddNode *>> cache;
  SddNode *card_k1 = CardinalityK(8, 4, sdd_manager, &cache);
  std::unordered_map<uint32_t, uint32_t> variable_mapping;
  for (auto i = 1; i <= 8; ++i) {
    variable_mapping[(uint32_t) i] = (uint32_t) i;
  }
  PsddNode *node_1 = manager->ConvertSddToPsdd(card_k1, sdd_manager_vtree(sdd_manager), 0, variable_mapping);
  PsddNode *node_2 = manager->ConvertSddToPsdd(card_k1, sdd_manager_vtree(sdd_manager), 1, variable_mapping);
  auto serialized_node_1 = psdd_node_util::SerializePsddNodes(node_1);
  auto serialized_node_2 = psdd_node_util::SerializePsddNodes(node_2);
  auto result = manager->Multiply(node_1, node_2, 3);
  std::bitset<MAX_VAR> mask = (1 << 9) - 1;
  auto result_s_psdd = psdd_node_util::SerializePsddNodes(result.first);
  auto cap = 1 << 9;
  for (auto i = 0; i < cap; ++i) {
    std::bitset<MAX_VAR> cur_instantiation = i;
    PsddParameter cur_num = psdd_node_util::Evaluate(mask, cur_instantiation, serialized_node_1)
        * psdd_node_util::Evaluate(mask, cur_instantiation, serialized_node_2);
    PsddParameter result_num = psdd_node_util::Evaluate(mask, cur_instantiation, result_s_psdd) * result.second;
    EXPECT_DOUBLE_EQ(cur_num.parameter(), result_num.parameter());
  }
}

TEST(PSDD_MANAGER_TEST, MULTIPLY_TEST3) {
  RandomDoubleFromGammaGenerator generator(1, 1, 0);
  Vtree *vtree = sdd_vtree_new(8, "right");
  PsddManager *manager = PsddManager::GetPsddManagerFromVtree(vtree);
  SddManager *sdd_manager = sdd_manager_new(vtree);
  sdd_vtree_free(vtree);
  std::unordered_map<uint32_t, std::unordered_map<uint32_t, SddNode *>> cache;
  SddNode *card_k1 = CardinalityK(8, 4, sdd_manager, &cache);
  std::unordered_map<uint32_t, uint32_t> variable_mapping;
  for (auto i = 1; i <= 8; ++i) {
    variable_mapping[(uint32_t) i] = (uint32_t) i;
  }
  PsddNode *sdd_node_1 = manager->ConvertSddToPsdd(card_k1, sdd_manager_vtree(sdd_manager), 0, variable_mapping);
  PsddNode *sdd_node_2 = manager->NormalizePsddNode(manager->vtree(), manager->GetPsddLiteralNode(1, 1), 1);
  PsddNode *node_1 = manager->SampleParameters(&generator, sdd_node_1, 0);
  PsddNode *node_2 = manager->SampleParameters(&generator, sdd_node_2, 0);
  auto s_node_1 = psdd_node_util::SerializePsddNodes(node_1);
  auto s_node_2 = psdd_node_util::SerializePsddNodes(node_2);
  auto result = manager->Multiply(node_1, node_2, 3);
  std::bitset<MAX_VAR> mask = (1 << 9) - 1;
  auto result_s_psdd = psdd_node_util::SerializePsddNodes(result.first);
  auto cap = 1 << 9;
  for (auto i = 0; i < cap; ++i) {
    std::bitset<MAX_VAR> cur_instantiation = i;
    PsddParameter cur_num = psdd_node_util::Evaluate(mask, cur_instantiation, s_node_1)
        * psdd_node_util::Evaluate(mask, cur_instantiation, s_node_2);
    PsddParameter result_num = psdd_node_util::Evaluate(mask, cur_instantiation, result_s_psdd) * result.second;
    EXPECT_DOUBLE_EQ(cur_num.parameter(), result_num.parameter());
  }
}

TEST(PSDD_MANAGER_TEST, MULTIPLY_TEST4) {
  RandomDoubleFromGammaGenerator generator(1, 1, 0);
  Vtree *vtree = sdd_vtree_new(8, "right");
  PsddManager *manager = PsddManager::GetPsddManagerFromVtree(vtree);
  SddManager *sdd_manager = sdd_manager_new(vtree);
  sdd_vtree_free(vtree);
  std::unordered_map<uint32_t, std::unordered_map<uint32_t, SddNode *>> cache;
  std::vector<SddNode *> cards;
  for (auto i = 0; i <= 8; ++i) {
    cards.push_back(CardinalityK(8, i, sdd_manager, &cache));
  }
  SddNode *less6 = sdd_manager_false(sdd_manager);
  for (auto i = 0; i < 6; ++i) {
    less6 = sdd_disjoin(less6, cards[i], sdd_manager);
  }
  SddNode *bigger3 = sdd_manager_false(sdd_manager);
  for (auto i = 3; i <= 8; ++i) {
    bigger3 = sdd_disjoin(bigger3, cards[i], sdd_manager);
  }
  std::unordered_map<uint32_t, uint32_t> variable_mapping;
  for (auto i = 1; i <= 8; ++i) {
    variable_mapping[(uint32_t) i] = (uint32_t) i;
  }
  PsddNode *sdd_node_1 = manager->ConvertSddToPsdd(less6, sdd_manager_vtree(sdd_manager), 0, variable_mapping);
  PsddNode *sdd_node_2 = manager->ConvertSddToPsdd(bigger3, sdd_manager_vtree(sdd_manager), 1, variable_mapping);
  PsddNode *node_1 = manager->SampleParameters(&generator, sdd_node_1, 0);
  PsddNode *node_2 = manager->SampleParameters(&generator, sdd_node_2, 0);
  auto s_node_1 = psdd_node_util::SerializePsddNodes(node_1);
  auto s_node_2 = psdd_node_util::SerializePsddNodes(node_2);
  auto result = manager->Multiply(node_1, node_2, 3);
  std::bitset<MAX_VAR> mask = (1 << 9) - 1;
  auto result_s_psdd = psdd_node_util::SerializePsddNodes(result.first);
  auto cap = 1 << 9;
  for (auto i = 0; i < cap; ++i) {
    std::bitset<MAX_VAR> cur_instantiation = i;
    PsddParameter cur_num = psdd_node_util::Evaluate(mask, cur_instantiation, s_node_1)
        * psdd_node_util::Evaluate(mask, cur_instantiation, s_node_2);
    PsddParameter result_num = psdd_node_util::Evaluate(mask, cur_instantiation, result_s_psdd) * result.second;
    EXPECT_DOUBLE_EQ(cur_num.parameter(), result_num.parameter());
  }
}

