//
// Created by jason on 2/28/18.
//

#include <fstream>
#include <sstream>
#include <cmath>
#include <stack>
#include <cassert>
#include <queue>
#include "cnf.h"
#include "psdd_unique_table.h"
extern "C" {
#include "sddapi.h"
}

namespace {
std::vector<uint32_t> VtreeVariables(Vtree *v) {
  std::vector<uint32_t> variables;
  std::stack<Vtree *> vtree_nodes;
  vtree_nodes.push(v);
  while (!vtree_nodes.empty()) {
    Vtree *v = vtree_nodes.top();
    vtree_nodes.pop();
    if (sdd_vtree_is_leaf(v)) {
      variables.push_back((uint32_t) sdd_vtree_var(v));
    } else {
      vtree_nodes.push(sdd_vtree_left(v));
      vtree_nodes.push(sdd_vtree_right(v));
    }
  }
  return variables;
}

Vtree *VtreeSwapVariables(Vtree *v, const std::unordered_map<int, int> &variable_mapping) {
  std::vector<Vtree *> vtree_nodes;
  vtree_nodes.push_back(v);
  size_t index = 0;
  while (index < vtree_nodes.size()) {
    Vtree *cur_vtree_node = vtree_nodes[index];
    if (!sdd_vtree_is_leaf(cur_vtree_node)) {
      vtree_nodes.push_back(sdd_vtree_left(cur_vtree_node));
      vtree_nodes.push_back(sdd_vtree_right(cur_vtree_node));
    }
    index++;
  }
  std::unordered_map<size_t, Vtree *> vtree_construction_cache;
  for (auto vtree_node_it = vtree_nodes.rbegin(); vtree_node_it != vtree_nodes.rend(); ++vtree_node_it) {
    Vtree *cur_vtree_node = *vtree_node_it;
    if (sdd_vtree_is_leaf(cur_vtree_node)) {
      auto variable_index = (int) sdd_vtree_var(cur_vtree_node);
      assert(variable_mapping.find(variable_index) != variable_mapping.end());
      auto new_variable_index = variable_mapping.find(variable_index)->second;
      vtree_construction_cache[(size_t) cur_vtree_node] = new_leaf_vtree(new_variable_index);
    } else {
      Vtree *left_vtree_node = sdd_vtree_left(cur_vtree_node);
      Vtree *right_vtree_node = sdd_vtree_right(cur_vtree_node);
      vtree_construction_cache[(size_t) cur_vtree_node] =
          new_internal_vtree(vtree_construction_cache[(size_t) left_vtree_node],
                             vtree_construction_cache[(size_t) right_vtree_node]);
    }
  }
  set_vtree_properties(vtree_construction_cache[(size_t)v]);
  return vtree_construction_cache[(size_t) v];
}

void TagVtree(Vtree *vtree_from_sdd_manager, Vtree *psdd_vtree) {
  std::stack<Vtree *> vtree_from_sdd_manager_stack;
  std::stack<Vtree *> psdd_vtree_stack;
  vtree_from_sdd_manager_stack.push(vtree_from_sdd_manager);
  psdd_vtree_stack.push(psdd_vtree);
  while (!vtree_from_sdd_manager_stack.empty()) {
    Vtree *cur_vtree_from_sdd_manager = vtree_from_sdd_manager_stack.top();
    Vtree *cur_psdd_vtree = psdd_vtree_stack.top();
    vtree_from_sdd_manager_stack.pop();
    psdd_vtree_stack.pop();
    sdd_vtree_set_data((void *) cur_psdd_vtree, cur_vtree_from_sdd_manager);
    if (!sdd_vtree_is_leaf(cur_vtree_from_sdd_manager)) {
      vtree_from_sdd_manager_stack.push(sdd_vtree_left(cur_vtree_from_sdd_manager));
      vtree_from_sdd_manager_stack.push(sdd_vtree_right(cur_vtree_from_sdd_manager));
      psdd_vtree_stack.push(sdd_vtree_left(cur_psdd_vtree));
      psdd_vtree_stack.push(sdd_vtree_right(cur_psdd_vtree));
    }
  }
}

SddNode *ConstructSddFromCnf(const std::vector<std::vector<int>> &clauses,
                             const std::unordered_map<int, int> &variable_mapping,
                             SddManager *manager) {
  SddNode* total_logic = sdd_manager_true(manager);
  for (const auto &cur_clause : clauses) {
    SddNode* cur_clause_sdd = sdd_manager_false(manager);
    for (int lit : cur_clause){
      if (lit < 0){
        int psdd_variable_index = -lit;
        int sdd_variable_index = variable_mapping.find(psdd_variable_index)->second;
        SddNode* cur_lit = sdd_manager_literal(-sdd_variable_index, manager);
        cur_clause_sdd = sdd_disjoin(cur_lit, cur_clause_sdd, manager);
      }else{
        int sdd_variable_index = variable_mapping.find(lit)->second;
        SddNode* cur_lit = sdd_manager_literal(sdd_variable_index, manager);
        cur_clause_sdd = sdd_disjoin(cur_lit, cur_clause_sdd, manager);
      }
    }
    total_logic = sdd_conjoin(total_logic, cur_clause_sdd, manager);
  }
  return total_logic;
}

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
  if (sdd_node_is_false(root_node)){
    return nullptr;
  }
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
}
CNF::CNF(const char *filename) {
  std::ifstream cnf_file(filename);
  std::string line;
  while (getline(cnf_file, line)) {
    if (line[0] == 'p') {
      continue;
    }
    std::vector<int> clause;
    std::stringstream ss(line);
    int token;
    while (ss >> token) {
      if (token == 0) {
        break;
      }
      clause.push_back(token);
    }
    clauses_.push_back(clause);
  }
  cnf_file.close();
}
const std::vector<std::vector<int>> &CNF::clauses() const {
  return clauses_;
}

PsddNode *CNF::CompileToSddWithEvidence(const std::unordered_map<uint32_t, bool> &evid, Vtree *vtree) const {
  std::vector<std::vector<int>> new_clauses;
  auto vtree_variables = VtreeVariables(vtree);
  std::bitset<MAX_VAR> vtree_variables_mask;
  for (auto vtree_variable_index  : vtree_variables){
    vtree_variables_mask.set(vtree_variable_index);
  }
  for (auto it = evid.begin(); it != evid.end(); ++it){
    if (vtree_variables_mask[it->first]){
      if (it->second){
        new_clauses.emplace_back(std::vector<int>({(int)it->first}));
      }else {
        new_clauses.emplace_back(std::vector<int>({-(int)it->first}));
      }
    }
  }
  for (const auto &old_clause: clauses_) {
    std::vector<int> new_clause;
    bool subsumed = false;
    for (int lit : old_clause) {
      auto variable_index = (uint32_t) std::abs(lit);
      if (evid.find(variable_index) == evid.end()) {
        new_clause.push_back(lit);
      } else {
        if (evid.find(variable_index)->second == (lit > 0)) {
          subsumed = true;
          break;
        } else {
          continue;
        }
      }
    }
    if (subsumed) {
      continue;
    } else if (new_clause.empty()) {
      // empty clause
      return nullptr;
    } else {
      new_clauses.push_back(new_clause);
    }
  }
  std::vector<std::vector<int>> existential_quantified_clauses;
  for (const auto& new_clause: new_clauses){
    std::vector<int> quantified_clause;
    for (int lit : new_clause){
      auto variable_index = (uint32_t) std::abs(lit);
      if (vtree_variables_mask[variable_index]){
        quantified_clause.push_back(lit);
      }
    }
    if (!quantified_clause.empty()){
      existential_quantified_clauses.push_back(quantified_clause);
    }
  }
  size_t node_index = 0;
  PsddUniqueTable *unique_table = PsddUniqueTable::GetPsddUniqueTable();
  std::unordered_map<SddLiteral, PsddNode *> true_node_map;
  if (existential_quantified_clauses.empty()) {
    // no constraint, return true node
    PsddNode *result_node = GetTrueNode(vtree, 1, &node_index, unique_table, &true_node_map);
    delete (unique_table);
    return result_node;
  } else {
    std::unordered_map<int, int> variable_mapping;
    auto variable_size = vtree_variables.size();
    for (auto i = 1; i <= variable_size; ++i){
      variable_mapping[vtree_variables[i-1]] = i;
    }
    Vtree* sdd_vtree = VtreeSwapVariables(vtree, variable_mapping);
    SddManager* manager = sdd_manager_new(sdd_vtree);
    sdd_vtree_free(sdd_vtree);
    sdd_vtree = sdd_manager_vtree(manager);
    TagVtree(sdd_vtree, vtree);
    sdd_manager_auto_gc_and_minimize_off(manager);
    SddNode* constraint_with_sdd_variables = ConstructSddFromCnf(existential_quantified_clauses, variable_mapping, manager);
    PsddNode* converted_psdd = ConvertSddToPsdd(constraint_with_sdd_variables, vtree, unique_table, & node_index, 1);
    delete(unique_table);
    sdd_manager_free(manager);
    return converted_psdd;
  }
}
CNF::CNF(const std::vector<std::vector<int>> &clauses): clauses_(clauses){}
bool CNF::CheckConstraintWithPartialInstantiation(const std::bitset<MAX_VAR> &variable_mask,
                                                  const std::bitset<MAX_VAR> &variable_instantiation) const {
  for (const auto& clause : clauses_){
    bool need_check = true;
    for (int lit : clause){
      auto variable_index = (size_t) std::abs(lit);
      if (!variable_mask[variable_index]){
        need_check = false;
      }
    }
    if (need_check){
      bool satisfied = false;
      for (int lit: clause){
        auto variable_index = (size_t) std::abs(lit);
        if (variable_instantiation[variable_index] == (lit > 0)){
          satisfied = true;
          break;
        }
      }
      if (! satisfied){
        return false;
      }
    }
  }
  return true;
}

