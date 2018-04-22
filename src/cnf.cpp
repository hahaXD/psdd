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
SddNode *ConstructSddFromCnf(const std::vector<std::vector<SddLiteral>> &clauses,
                             const std::unordered_map<SddLiteral, SddLiteral> &variable_mapping,
                             SddManager *manager) {
  SddNode *total_logic = sdd_manager_true(manager);
  for (const auto &cur_clause : clauses) {
    SddNode *cur_clause_sdd = sdd_manager_false(manager);
    for (SddLiteral lit : cur_clause) {
      if (lit < 0) {
        SddLiteral psdd_variable_index = -lit;
        SddLiteral sdd_variable_index = variable_mapping.find(psdd_variable_index)->second;
        SddNode *cur_lit = sdd_manager_literal(-sdd_variable_index, manager);
        cur_clause_sdd = sdd_disjoin(cur_lit, cur_clause_sdd, manager);
      } else {
        SddLiteral sdd_variable_index = variable_mapping.find(lit)->second;
        SddNode *cur_lit = sdd_manager_literal(sdd_variable_index, manager);
        cur_clause_sdd = sdd_disjoin(cur_lit, cur_clause_sdd, manager);
      }
    }
    total_logic = sdd_conjoin(total_logic, cur_clause_sdd, manager);
  }
  return total_logic;
}
}
CNF::CNF(const char *filename) {
  std::ifstream cnf_file(filename);
  std::string line;
  while (getline(cnf_file, line)) {
    if (line[0] == 'p') {
      continue;
    }
    std::vector<SddLiteral> clause;
    std::stringstream ss(line);
    SddLiteral token;
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
const std::vector<std::vector<SddLiteral>> &CNF::clauses() const {
  return clauses_;
}
/*
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
 */
CNF::CNF(const std::vector<std::vector<SddLiteral>> &clauses) : clauses_(clauses) {}

PsddNode *CNF::Compile(PsddManager *psdd_manager, uintmax_t flag_index) const {
  Vtree *psdd_vtree = psdd_manager->vtree();
  SddLiteral sdd_index = 1;
  std::unordered_map<SddLiteral, SddLiteral> psdd_variable_to_sdd_variable;
  std::unordered_map<uint32_t, uint32_t> sdd_variable_to_psdd_variable;
  std::vector<SddLiteral> variables_in_psdd = vtree_util::VariablesUnderVtree(psdd_vtree);
  for (SddLiteral psdd_variable_index : variables_in_psdd) {
    psdd_variable_to_sdd_variable[psdd_variable_index] = sdd_index;
    sdd_variable_to_psdd_variable[(uint32_t) sdd_index] = (uint32_t) psdd_variable_index;
    sdd_index += 1;
  }
  Vtree *sdd_vtree = vtree_util::CopyVtree(psdd_vtree, psdd_variable_to_sdd_variable);
  SddManager *new_sdd_manager = sdd_manager_new(sdd_vtree);
  sdd_vtree_free(sdd_vtree);
  sdd_manager_auto_gc_and_minimize_off(new_sdd_manager);
  SddNode *result = ConstructSddFromCnf(clauses_, psdd_variable_to_sdd_variable, new_sdd_manager);
  PsddNode *psdd_result = psdd_manager->ConvertSddToPsdd(result,
                                                         sdd_manager_vtree(new_sdd_manager),
                                                         flag_index,
                                                         sdd_variable_to_psdd_variable);
  sdd_manager_free(new_sdd_manager);
  return psdd_result;
}

/*
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
*/