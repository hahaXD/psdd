//
// Created by Yujia Shen on 10/16/17.
//
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <psdd_node.h>
#include <vector>
#include <stack>
#include <psdd_unique_table.h>
extern "C" {
#include "sddapi.h"
}

namespace {
PsddNode *GetTrueNode(Vtree *target_vtree_node,
                      uintmax_t flag_index,
                      uintmax_t *node_index,
                      PsddUniqueTable *unique_table,
                      std::unordered_map<SddLiteral, PsddNode *> *true_node_map) {
  if (true_node_map->find(sdd_vtree_position(target_vtree_node)) != true_node_map->end()) {
    return true_node_map->find(sdd_vtree_position(target_vtree_node))->second;
  } else {
    std::vector<Vtree *> post_order_vtree_nodes;
    std::stack<Vtree *> vtree_node_stack;
    vtree_node_stack.push(target_vtree_node);
    while (!vtree_node_stack.empty()) {
      Vtree *cur_vtree_node = vtree_node_stack.top();
      vtree_node_stack.pop();
      post_order_vtree_nodes.push_back(cur_vtree_node);
      if (!sdd_vtree_is_leaf(cur_vtree_node)) {
        Vtree *left_vtree_node = sdd_vtree_left(cur_vtree_node);
        Vtree *right_vtree_node = sdd_vtree_right(cur_vtree_node);
        if (true_node_map->find(sdd_vtree_position(left_vtree_node)) == true_node_map->end()) {
          // no true node constructed for left_vtree_node
          vtree_node_stack.push(left_vtree_node);
        }
        if (true_node_map->find(sdd_vtree_position(right_vtree_node)) == true_node_map->end()) {
          // no true node constructed for right vtree_node
          vtree_node_stack.push(right_vtree_node);
        }
      }
    }
    for (auto it = post_order_vtree_nodes.rbegin(); it != post_order_vtree_nodes.rend(); it++) {
      Vtree *cur_vtree_node = *it;
      assert(true_node_map->find(sdd_vtree_position(cur_vtree_node)) == true_node_map->end());
      if (sdd_vtree_is_leaf(cur_vtree_node)) {
        PsddNode *new_true_node = new PsddTopNode(*node_index,
                                                  cur_vtree_node,
                                                  flag_index,
                                                  static_cast<uint32_t>(sdd_vtree_var(cur_vtree_node)),
                                                  PsddParameter::CreateFromDecimal(0),
                                                  PsddParameter::CreateFromDecimal(0));
        new_true_node = unique_table->GetUniqueNode(new_true_node, node_index);
        true_node_map->insert(std::make_pair(sdd_vtree_position(cur_vtree_node), new_true_node));
      } else {
        Vtree *cur_left_node = sdd_vtree_left(cur_vtree_node);
        Vtree *cur_right_node = sdd_vtree_right(cur_vtree_node);
        assert(true_node_map->find(sdd_vtree_position(cur_left_node)) != true_node_map->end());
        assert(true_node_map->find(sdd_vtree_position(cur_right_node)) != true_node_map->end());
        PsddNode *left_true_node = true_node_map->find(sdd_vtree_position(cur_left_node))->second;
        PsddNode *right_true_node = true_node_map->find(sdd_vtree_position(cur_right_node))->second;
        PsddNode *new_true_node =
            new PsddDecisionNode(*node_index, cur_vtree_node, flag_index, {left_true_node}, {right_true_node}, {});
        new_true_node = unique_table->GetUniqueNode(new_true_node, node_index);
        true_node_map->insert(std::make_pair(sdd_vtree_position(cur_vtree_node), new_true_node));
      }
    }
    assert(true_node_map->find(sdd_vtree_position(target_vtree_node)) != true_node_map->end());
    return true_node_map->find(sdd_vtree_position(target_vtree_node))->second;
  }
}

PsddNode *NormalizePsddNode(Vtree *target_vtree_node,
                            PsddNode *target_psdd_node,
                            uintmax_t flag_index,
                            uintmax_t *node_index,
                            PsddUniqueTable *unique_table,
                            std::unordered_map<SddLiteral, PsddNode *> *true_node_map) {
  PsddNode *cur_node = target_psdd_node;
  while (cur_node->vtree_node() != target_vtree_node) {
    Vtree *cur_vtree_node = cur_node->vtree_node();
    Vtree *cur_vtree_parent_node = sdd_vtree_parent(cur_vtree_node);
    assert(cur_vtree_parent_node != nullptr);
    if (sdd_vtree_left(cur_vtree_parent_node) == cur_vtree_node) {
      auto true_node =
          GetTrueNode(sdd_vtree_right(cur_vtree_parent_node), flag_index, node_index, unique_table, true_node_map);
      PsddNode *next_node =
          new PsddDecisionNode(*node_index, cur_vtree_parent_node, flag_index, {cur_node}, {true_node}, {});
      next_node = unique_table->GetUniqueNode(next_node, node_index);
      cur_node = next_node;
    } else {
      assert(sdd_vtree_right(cur_vtree_parent_node) == cur_vtree_node);
      auto true_node =
          GetTrueNode(sdd_vtree_left(cur_vtree_parent_node), flag_index, node_index, unique_table, true_node_map);
      PsddNode *next_node =
          new PsddDecisionNode(*node_index, cur_vtree_parent_node, flag_index, {true_node}, {cur_node}, {});
      next_node = unique_table->GetUniqueNode(next_node, node_index);
      cur_node = next_node;
    }
  }
  return cur_node;
}

// Inside the SDD node, it contains a vtree node whose user data maps to the new vtree node under the target vtree. For constructing leaf SDD nodes, we use the variable indexes embedded in the new vtree node.
// Given a SDD literal node n, the new variable index can be get by sdd_vtree_var((Vtree*) sdd_vtree_data(sdd_vtree_of(n)))
PsddNode *ConvertSddToPsdd(SddNode *root_node,
                           Vtree *target_vtree,
                           PsddUniqueTable *unique_table,
                           uintmax_t *node_index,
                           uintmax_t flag_index) {
  uintmax_t cur_flag_index = flag_index;
  assert(!sdd_node_is_false(root_node));
  std::unordered_map<SddLiteral, PsddNode *> true_nodes_map;
  if (sdd_node_is_true(root_node)) {
    return GetTrueNode(target_vtree, cur_flag_index, node_index, unique_table, &true_nodes_map);
  }
  SddSize number_of_nodes = 0;
  SddNode **node_list = sdd_topological_sort(root_node, &number_of_nodes);
  // Key is the vtree index.
  std::unordered_map<SddSize, PsddNode *> node_map;
  for (SddSize i = 0; i < number_of_nodes; i++) {
    SddNode *cur_node = node_list[i];
    if (sdd_node_is_decision(cur_node)) {
      Vtree *old_vtree_node = sdd_vtree_of(cur_node);
      auto new_vtree_node = (Vtree *) sdd_vtree_data(old_vtree_node);
      std::vector<PsddNode *> primes;
      std::vector<PsddNode *> subs;
      SddNode **elements = sdd_node_elements(cur_node);
      SddSize element_size = sdd_node_size(cur_node);
      for (auto j = 0; j < element_size; j++) {
        SddNode *cur_prime = elements[2 * j];
        SddNode *cur_sub = elements[2 * j + 1];
        assert(node_map.find(sdd_id(cur_prime)) != node_map.end());
        PsddNode *cur_psdd_prime = node_map[sdd_id(cur_prime)];
        if (sdd_node_is_true(cur_sub)) {
          PsddNode *cur_normed_psdd_prime = NormalizePsddNode(sdd_vtree_left(new_vtree_node),
                                                              cur_psdd_prime,
                                                              cur_flag_index,
                                                              node_index,
                                                              unique_table,
                                                              &true_nodes_map);
          PsddNode *cur_normed_psdd_sub =
              GetTrueNode(sdd_vtree_right(new_vtree_node), cur_flag_index, node_index, unique_table, &true_nodes_map);
          primes.push_back(cur_normed_psdd_prime);
          subs.push_back(cur_normed_psdd_sub);
        } else if (sdd_node_is_false(cur_sub)) {
          continue;
        } else {
          // a literal or decision
          assert(node_map.find(sdd_id(cur_sub)) != node_map.end());
          PsddNode *cur_psdd_sub = node_map[sdd_id(cur_sub)];
          PsddNode *cur_normed_psdd_prime = NormalizePsddNode(sdd_vtree_left(new_vtree_node),
                                                              cur_psdd_prime,
                                                              cur_flag_index,
                                                              node_index,
                                                              unique_table,
                                                              &true_nodes_map);
          PsddNode *cur_normed_psdd_sub = NormalizePsddNode(sdd_vtree_right(new_vtree_node),
                                                            cur_psdd_sub,
                                                            cur_flag_index,
                                                            node_index,
                                                            unique_table,
                                                            &true_nodes_map);
          primes.push_back(cur_normed_psdd_prime);
          subs.push_back(cur_normed_psdd_sub);
        }
      }
      assert(!primes.empty());
      PsddNode *new_decn_node = new PsddDecisionNode(*node_index, new_vtree_node, cur_flag_index, primes, subs, {});
      new_decn_node = unique_table->GetUniqueNode(new_decn_node, node_index);
      node_map[sdd_id(cur_node)] = new_decn_node;
    } else if (sdd_node_is_literal(cur_node)) {
      Vtree *old_vtree_node = sdd_vtree_of(cur_node);
      auto new_vtree_node = (Vtree *) sdd_vtree_data(old_vtree_node);
      SddLiteral old_literal = sdd_node_literal(cur_node);
      int32_t new_literal = 0;
      if (old_literal > 0) {
        // a positive literal
        new_literal = static_cast<int32_t>(sdd_vtree_var(new_vtree_node));
      } else {
        new_literal = -static_cast<int32_t>(sdd_vtree_var(new_vtree_node));
      }
      PsddNode *new_literal_node = new PsddLiteralNode(*node_index, new_vtree_node, cur_flag_index, new_literal);
      new_literal_node = unique_table->GetUniqueNode(new_literal_node, node_index);
      node_map[sdd_id(cur_node)] = new_literal_node;
    } else {
      // true or false node
      continue;
    }
  }
  assert(node_map.find(sdd_id(root_node)) != node_map.end());
  PsddNode *psdd_root_node = node_map[sdd_id(root_node)];
  PsddNode *psdd_root_normalized_node =
      NormalizePsddNode(target_vtree, psdd_root_node, cur_flag_index, node_index, unique_table, &true_nodes_map);
  free(node_list);
  return psdd_root_normalized_node;
}

SddNode *GenerateCardinalitySDD(const std::vector<SddLiteral> &variables,
                                SddLiteral cardinality,
                                SddManager *manager) {
  std::vector<std::vector<SddNode *>> cardinality_cache;
  SddNode *neg_first = sdd_manager_literal(-variables[0], manager);
  sdd_ref(neg_first, manager);
  SddNode *pos_first = sdd_manager_literal(variables[0], manager);
  sdd_ref(pos_first, manager);
  std::vector<SddNode *> first_node_cache((size_t) cardinality + 1, nullptr);
  first_node_cache[0] = neg_first;
  first_node_cache[1] = pos_first;
  cardinality_cache.push_back(first_node_cache);
  auto variable_size = variables.size();
  for (auto i = 1; i < variable_size; ++i) {
    SddLiteral cur_variable_index = variables[i];
    const auto &last_node_cache = cardinality_cache[i - 1];
    size_t upper_bound = std::min((size_t) cardinality, (size_t) i + 1);
    SddNode *pos_node = sdd_manager_literal(cur_variable_index, manager);
    sdd_ref(pos_node, manager);
    SddNode *neg_node = sdd_manager_literal(-cur_variable_index, manager);
    sdd_ref(neg_node, manager);
    std::vector<SddNode *> cur_node_cache((size_t) cardinality + 1, nullptr);
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
        SddNode *new_pos_node = sdd_conjoin(pos_node, last_node_cache[j - 1], manager);
        sdd_ref(new_pos_node, manager);
        SddNode *new_neg_node = sdd_conjoin(neg_node, last_node_cache[j], manager);
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
    size_t upper_bound = std::min((size_t) cardinality, (size_t) i + 1);
    for (auto j = 0; j <= upper_bound; ++j) {
      assert(cardinality_cache[i][j] != nullptr);
      sdd_deref(cardinality_cache[i][j], manager);
    }
  }
  return result_node;
}
class PsddMPETest : public testing::Test {
};
}

TEST_F(PsddMPETest, CardinalityTest) {
  Vtree *vtree = sdd_vtree_new(20, "balanced");
  SddManager *manager = sdd_manager_new(vtree);
  sdd_manager_auto_gc_and_minimize_off(manager);
  sdd_vtree_free(vtree);
  std::vector<SddLiteral> considered_vars;
  for (auto i = 1; i <= 20; ++i) {
    considered_vars.push_back(i);
  }
  SddNode *constraint_node = GenerateCardinalitySDD(considered_vars, 10, manager);
  PsddUniqueTable *unique_table = PsddUniqueTable::GetPsddUniqueTable();
  uintmax_t index = 0;
  Vtree* vtree_used = sdd_manager_vtree(manager);
  std::stack<Vtree*> stack_vtrees;
  stack_vtrees.push(vtree_used);
  while(!stack_vtrees.empty()){
    Vtree* cur_vtree = stack_vtrees.top();
    stack_vtrees.pop();
    sdd_vtree_set_data((void*)cur_vtree, cur_vtree);
    if (sdd_vtree_is_leaf(cur_vtree)){
      continue;
    }else{
      stack_vtrees.push(sdd_vtree_left(cur_vtree));
      stack_vtrees.push(sdd_vtree_right(cur_vtree));
    }
  }
  PsddNode *psdd_node = ConvertSddToPsdd(constraint_node, sdd_manager_vtree(manager), unique_table, &index, 0);
  std::vector<PsddNode *> serialized_nodes = psdd_node_util::SerializePsddNodes(psdd_node);
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
  SddNode* test_node = constraint_node;
  for (auto i = 1; i <=20; ++i){
    SddNode* tmp = nullptr;
    if (repetitive_data[i]){
      tmp = sdd_condition(i, test_node, manager);
    }else{
      tmp = sdd_condition(-i, test_node, manager);
    }
    test_node = tmp;
  }
  EXPECT_EQ(test_node, sdd_manager_true(manager));
  psdd_node_util::SetActivationFlag(repetitive_data,serialized_nodes);
  EXPECT_TRUE(psdd_node->IsConsistent(repetitive_data, 20));
  EXPECT_TRUE(serialized_nodes[0]->activation_flag());
  std::stack<PsddNode*> to_do;
  to_do.push(psdd_node);
  while (!to_do.empty()){
    PsddNode* cur_node = to_do.top();
    to_do.pop();
    if (cur_node->node_type() == LITERAL_NODE_TYPE){
      continue;
    }else if (cur_node->node_type() == TOP_NODE_TYPE){
      PsddTopNode* cur_top_node = cur_node->psdd_top_node();
      uintmax_t variable_index = cur_top_node->variable_index();
      if (repetitive_data[variable_index]){
        cur_top_node->IncrementTrueDataCount(100);
      }else{
        cur_top_node->IncrementFalseDataCount(100);
      }
    }else{
      PsddDecisionNode* cur_decn_node = cur_node->psdd_decision_node();
      const std::vector<PsddNode*>& cur_primes = cur_decn_node->primes();
      const std::vector<PsddNode*>& cur_subs = cur_decn_node->subs();
      uintmax_t element_size = cur_primes.size();
      for (auto i = 0 ; i < element_size; ++i){
        PsddNode* prime_node = cur_primes[i];
        PsddNode* sub_node = cur_subs[i];
        if(prime_node->activation_flag() && sub_node->activation_flag()){
          to_do.push(prime_node);
          to_do.push(sub_node);
          cur_decn_node->IncrementDataCount((uintmax_t)i, 100);
        }
      }
    }
  }
  for (PsddNode *cur_node : serialized_nodes) {
    cur_node->ResetActivationFlag();
    cur_node->CalculateParametersUsingLaplacianSmoothing(PsddParameter::CreateFromDecimal(0.2));
  }
  sdd_manager_free(manager);
  unique_table->DeleteUnusedPsddNodes({});
  delete (unique_table);
  auto result = psdd_node_util::GetMPESolution(psdd_node);
  EXPECT_EQ(result.first, repetitive_data);
}
