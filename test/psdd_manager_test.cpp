//
// Created by Yujia Shen on 4/5/18.
//
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <unordered_map>
#include <psdd_manager.h>
extern "C"{
#include "sddapi.h"
}

namespace {
SddNode* CardinalityK(uint32_t variable_size_usign, uint32_t k, SddManager* manager, std::unordered_map<uint32_t, std::unordered_map<uint32_t, SddNode*>>* cache){
  SddLiteral variable_size = (SddLiteral) variable_size_usign;
  auto cache_it = cache->find(variable_size_usign);
  if (cache_it != cache->end()){
    auto second_cache_it = cache_it->second.find(k);
    if (second_cache_it != cache_it->second.end()){
      return second_cache_it->second;
    }
  }else{
    cache->insert({variable_size, {}});
    cache_it = cache->find(variable_size_usign);
  }
  if (variable_size == 1){
    if (k==0){
      SddNode* result = sdd_manager_literal(-1, manager);
      cache_it->second.insert({0,result});
      return result;
    }else{
      SddNode* result = sdd_manager_literal(1, manager);
      cache_it->second.insert({1, result});
      return result;
    }
  }else{
    if (k == 0) {
      SddNode *remaining = CardinalityK(variable_size_usign - 1, 0, manager, cache);
      SddNode *result = sdd_conjoin(sdd_manager_literal(-variable_size, manager), remaining, manager);
      cache_it->second.insert({0, result});
      return result;
    }else if (k == variable_size){
      SddNode* remaining = CardinalityK(variable_size_usign-1, k-1, manager, cache);
      SddNode* result = sdd_conjoin(sdd_manager_literal(variable_size, manager), remaining, manager);
      cache_it->second.insert({k, result});
      return result;
    }else{
      SddNode* remaining_positive = CardinalityK(variable_size_usign-1, k-1, manager, cache);
      SddNode* remaining_negative = CardinalityK(variable_size_usign-1, k, manager, cache);
      SddNode* positive_case = sdd_conjoin(sdd_manager_literal(variable_size, manager), remaining_positive, manager);
      SddNode* negative_case = sdd_conjoin(sdd_manager_literal(-variable_size, manager), remaining_negative, manager);
      SddNode* result = sdd_disjoin(positive_case, negative_case, manager);
      cache_it->second.insert({k, result});
      return result;
    }
  }
}

}

TEST(SDD_TO_PSDD_TEST, MODEL_COUNT_TEST){
  Vtree* vtree = sdd_vtree_new(10, "right");
  SddManager* sdd_manager = sdd_manager_new(vtree);
  sdd_vtree_free(vtree);
  sdd_manager_auto_gc_and_minimize_off(sdd_manager);
  std::unordered_map<uint32_t, std::unordered_map<uint32_t, SddNode*>> cache;
  SddNode* cardinality_node = CardinalityK(10, 5, sdd_manager, &cache);
  std::unordered_map<uint32_t, uint32_t> variable_mapping;
  for (auto i = 0 ; i < 10; ++i){
    variable_mapping[(uint32_t)i+1] = (uint32_t)i+1;
  }
  PsddManager* psdd_manager = PsddManager::GetPsddManager(sdd_manager_vtree(sdd_manager), variable_mapping);
  PsddNode* result_psdd = psdd_manager->ConvertSddToPsdd(cardinality_node, sdd_manager_vtree(sdd_manager), 0, variable_mapping);
  std::vector<PsddNode*> serialized_nodes = psdd_node_util::SerializePsddNodes(result_psdd);
  EXPECT_EQ(sdd_global_model_count(cardinality_node, sdd_manager), psdd_node_util::ModelCount(serialized_nodes));
  delete psdd_manager;
  sdd_manager_free(sdd_manager);
}

TEST(SDD_TO_PSDD_TEST, VARIABLE_MAP_TEST){
  Vtree* vtree = sdd_vtree_new(10, "right");
  SddManager* sdd_manager = sdd_manager_new(vtree);
  sdd_vtree_free(vtree);
  sdd_manager_auto_gc_and_minimize_off(sdd_manager);
  std::unordered_map<uint32_t, std::unordered_map<uint32_t, SddNode*>> cache;
  SddNode* first_true = sdd_manager_literal(1, sdd_manager);
  std::unordered_map<uint32_t, uint32_t> variable_mapping;
  for (auto i = 0 ; i < 10; ++i){
    variable_mapping[(uint32_t)i+1] = (uint32_t)i+11;
  }
  PsddManager* psdd_manager = PsddManager::GetPsddManager(sdd_manager_vtree(sdd_manager), variable_mapping);
  PsddNode* result_psdd = psdd_manager->ConvertSddToPsdd(first_true, sdd_manager_vtree(sdd_manager), 0, variable_mapping);
  auto serialized_psdd = psdd_node_util::SerializePsddNodes(result_psdd);
  {
    std::bitset<MAX_VAR> variable_mask;
    variable_mask.set(11);
    std::bitset<MAX_VAR> variable_instantiation;
    EXPECT_TRUE(!psdd_node_util::IsConsistent(serialized_psdd, variable_mask, variable_instantiation));
    EXPECT_TRUE(psdd_node_util::IsConsistent(serialized_psdd, variable_mask, variable_mask));
  }
  for (auto i = 12; i <= 20; ++i){
    std::bitset<MAX_VAR> variable_instantiation;
    std::bitset<MAX_VAR> variable_mask;
    variable_mask.set((uint32_t)i);
    EXPECT_TRUE(psdd_node_util::IsConsistent(serialized_psdd, variable_mask, variable_instantiation));
    EXPECT_TRUE(psdd_node_util::IsConsistent(serialized_psdd, variable_mask, variable_mask));
  }
  delete psdd_manager;
  sdd_manager_free(sdd_manager);
}

