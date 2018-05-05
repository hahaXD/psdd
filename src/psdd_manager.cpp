//
// Created by Yujia Shen on 3/20/18.
//

#include <queue>
#include <stack>
#include <cassert>
#include <functional>
#include <fstream>
#include <iostream>
#include <sstream>
#include "psdd_manager.h"
#include "psdd_unique_table.h"
namespace {

void TagSddVtreeWithPsddVtree(const std::vector<Vtree *> &sdd_vtree_serialized,
                              const std::vector<Vtree *> &psdd_vtree_serialized,
                              const std::unordered_map<uint32_t, uint32_t> &variable_map) {
  auto vtree_size = sdd_vtree_serialized.size();
  assert(vtree_size == psdd_vtree_serialized.size());
  for (auto i = 0; i < vtree_size; ++i) {
    sdd_vtree_set_data((void *) psdd_vtree_serialized[i], sdd_vtree_serialized[i]);
  }
}

struct MultiplyFunctional {
  std::size_t operator()(const std::pair<PsddNode *, PsddNode *> &arg) const {
    return (std::hash<uintmax_t>{}(arg.first->node_index()) << 1) ^ (std::hash<uintmax_t>{}(arg.second->node_index()));
  }
  bool operator()(const std::pair<PsddNode *, PsddNode *> &arg1, const std::pair<PsddNode *, PsddNode *> &arg2) const {
    return (arg1.second == arg2.second) && (arg1.first == arg2.first);
  }
};

class ComputationCache {
 public:
  explicit ComputationCache(uint32_t variable_size) : cache_(2 * variable_size - 1) {}
  std::pair<PsddNode *, Probability> Lookup(PsddNode *first_node, PsddNode *second_node, bool *found) const {
    auto vtree_index = sdd_vtree_position(first_node->vtree_node());
    assert(cache_.size() > vtree_index);
    const auto &cache_at_vtree = cache_[vtree_index];
    auto node_pair = std::pair<PsddNode *, PsddNode *>(first_node, second_node);
    auto lookup_it = cache_at_vtree.find(node_pair);
    if (lookup_it == cache_at_vtree.end()) {
      *found = false;
      return std::make_pair(nullptr, Probability::CreateFromDecimal(0));
    } else {
      *found = true;
      return lookup_it->second;
    }
  }
  void Update(PsddNode *first, PsddNode *second, const std::pair<PsddNode *, Probability> &result) {
    auto vtree_index = sdd_vtree_position(first->vtree_node());
    assert(cache_.size() > vtree_index);
    std::pair<PsddNode *, PsddNode *> node_pair(first, second);
    cache_[vtree_index][node_pair] = result;
  }
 private:
  std::vector<std::unordered_map<std::pair<PsddNode *, PsddNode *>,
                                 std::pair<PsddNode *, Probability>,
                                 MultiplyFunctional,
                                 MultiplyFunctional>> cache_;
};

std::pair<PsddNode *, PsddParameter> MultiplyWithCache(PsddNode *first,
                                                       PsddNode *second,
                                                       PsddManager *manager,
                                                       uintmax_t flag_index,
                                                       ComputationCache *cache) {
  bool found = false;
  auto result = cache->Lookup(first, second, &found);
  if (found) return result;
  assert(sdd_vtree_position(first->vtree_node()) == sdd_vtree_position(second->vtree_node()));
  if (first->node_type() == DECISION_NODE_TYPE) {
    assert(second->node_type() == DECISION_NODE_TYPE);
    PsddDecisionNode *first_decision_node = first->psdd_decision_node();
    PsddDecisionNode *second_decision_node = second->psdd_decision_node();
    const auto &first_primes = first_decision_node->primes();
    const auto &first_subs = first_decision_node->subs();
    const auto &first_parameters = first_decision_node->parameters();
    const auto &second_primes = second_decision_node->primes();
    const auto &second_subs = second_decision_node->subs();
    const auto &second_parameters = second_decision_node->parameters();
    auto first_element_size = first_primes.size();
    auto second_element_size = second_primes.size();
    std::vector<PsddNode *> next_primes;
    std::vector<PsddNode *> next_subs;
    std::vector<PsddParameter> next_parameters;
    PsddParameter partition = PsddParameter::CreateFromDecimal(0);
    for (auto i = 0; i < first_element_size; ++i) {
      PsddNode *cur_first_prime = first_primes[i];
      PsddNode *cur_first_sub = first_subs[i];
      PsddParameter cur_first_param = first_parameters[i];
      for (auto j = 0; j < second_element_size; ++j) {
        PsddNode *cur_second_prime = second_primes[j];
        auto cur_prime_result = MultiplyWithCache(cur_first_prime, cur_second_prime, manager, flag_index, cache);
        if (cur_prime_result.first == nullptr) {
          continue;
        }
        PsddNode *cur_second_sub = second_subs[j];
        auto cur_sub_result = MultiplyWithCache(cur_first_sub, cur_second_sub, manager, flag_index, cache);
        if (cur_sub_result.first == nullptr) {
          continue;
        }
        next_primes.push_back(cur_prime_result.first);
        next_subs.push_back(cur_sub_result.first);
        PsddParameter cur_second_param = second_parameters[j];
        next_parameters.push_back(cur_second_param * cur_first_param * cur_prime_result.second * cur_sub_result.second);
        partition = partition + next_parameters.back();
      }
    }
    if (next_primes.empty()) {
      std::pair<PsddNode *, Probability> comp_result = {nullptr, PsddParameter::CreateFromDecimal(0)};
      cache->Update(first, second, comp_result);
      return comp_result;
    }
    for (auto &single_parameter : next_parameters) {
      single_parameter = single_parameter / partition;
      assert(single_parameter != PsddParameter::CreateFromDecimal(0));
    }
    auto new_node = manager->GetConformedPsddDecisionNode(next_primes, next_subs, next_parameters, flag_index);
    std::pair<PsddNode *, Probability> comp_result = {new_node, partition};
    cache->Update(first, second, comp_result);
    return comp_result;
  } else if (first->node_type() == LITERAL_NODE_TYPE) {
    PsddLiteralNode *first_literal_node = first->psdd_literal_node();
    if (second->node_type() == LITERAL_NODE_TYPE) {
      PsddLiteralNode *second_literal_node = second->psdd_literal_node();
      assert(first_literal_node->variable_index() == second_literal_node->variable_index());
      if (first_literal_node->literal() == second_literal_node->literal()) {
        PsddNode *new_node = manager->GetPsddLiteralNode(first_literal_node->literal(), flag_index);
        std::pair<PsddNode *, Probability> comp_result = {new_node, Probability::CreateFromDecimal(1)};
        cache->Update(first, second, comp_result);
        return comp_result;
      } else {
        std::pair<PsddNode *, Probability> comp_result = {nullptr, Probability::CreateFromDecimal(0)};
        cache->Update(first, second, comp_result);
        return comp_result;
      }
    } else {
      assert(second->node_type() == TOP_NODE_TYPE);
      PsddTopNode *second_top_node = second->psdd_top_node();
      assert(first_literal_node->variable_index() == second_top_node->variable_index());
      if (first_literal_node->sign()) {
        PsddNode *new_node = manager->GetPsddLiteralNode(first_literal_node->literal(), flag_index);
        std::pair<PsddNode *, Probability> comp_result = {new_node, second_top_node->true_parameter()};
        cache->Update(first, second, comp_result);
        return comp_result;
      } else {
        PsddNode *new_node = manager->GetPsddLiteralNode(first_literal_node->literal(), flag_index);
        std::pair<PsddNode *, Probability> comp_result = {new_node, second_top_node->false_parameter()};
        cache->Update(first, second, comp_result);
        return comp_result;
      }
    }
  } else {
    assert(first->node_type() == TOP_NODE_TYPE);
    PsddTopNode *first_top_node = first->psdd_top_node();
    if (second->node_type() == LITERAL_NODE_TYPE) {
      PsddLiteralNode *second_literal_node = second->psdd_literal_node();
      assert(first_top_node->variable_index() == second_literal_node->variable_index());
      if (second_literal_node->sign()) {
        PsddNode *new_node = manager->GetPsddLiteralNode(second_literal_node->literal(), flag_index);
        std::pair<PsddNode *, Probability> comp_result = {new_node, first_top_node->true_parameter()};
        cache->Update(first, second, comp_result);
        return comp_result;
      } else {
        PsddNode *new_node = manager->GetPsddLiteralNode(second_literal_node->literal(), flag_index);
        std::pair<PsddNode *, Probability> comp_result = {new_node, first_top_node->false_parameter()};
        cache->Update(first, second, comp_result);
        return comp_result;
      }
    } else {
      assert(second->node_type() == TOP_NODE_TYPE);
      PsddTopNode *second_top_node = second->psdd_top_node();
      assert(first_top_node->variable_index() == second_top_node->variable_index());
      PsddParameter pos_weight = first_top_node->true_parameter() * second_top_node->true_parameter();
      PsddParameter neg_weight = first_top_node->false_parameter() * second_top_node->false_parameter();
      PsddParameter partition = pos_weight + neg_weight;
      PsddNode *new_node = manager->GetPsddTopNode(first_top_node->variable_index(),
                                                   flag_index,
                                                   pos_weight / partition,
                                                   neg_weight / partition);
      assert(new_node->psdd_top_node()->true_parameter() != PsddParameter::CreateFromDecimal(0));
      assert(new_node->psdd_top_node()->false_parameter() != PsddParameter::CreateFromDecimal(0));
      std::pair<PsddNode *, Probability> comp_result = {new_node, partition};
      cache->Update(first, second, comp_result);
      return comp_result;
    }
  }
}
}

PsddManager *PsddManager::GetPsddManagerFromSddVtree(Vtree *sdd_vtree,
                                                     const std::unordered_map<uint32_t, uint32_t> &variable_mapping) {
  std::vector<Vtree *> serialized_sdd_vtree = vtree_util::SerializeVtree(sdd_vtree);
  for (auto vtree_it = serialized_sdd_vtree.rbegin(); vtree_it != serialized_sdd_vtree.rend(); ++vtree_it) {
    Vtree *sdd_vtree_node = *vtree_it;
    if (sdd_vtree_is_leaf(sdd_vtree_node)) {
      SddLiteral sdd_variable_index = sdd_vtree_var(sdd_vtree_node);
      auto variable_mapping_it = variable_mapping.find((uint32_t) sdd_variable_index);
      assert(variable_mapping_it != variable_mapping.end());
      Vtree *psdd_vtree_node = new_leaf_vtree((SddLiteral) variable_mapping_it->second);
      sdd_vtree_set_data((void *) psdd_vtree_node, sdd_vtree_node);
    } else {
      Vtree *sdd_vtree_left_node = sdd_vtree_left(sdd_vtree_node);
      Vtree *sdd_vtree_right_node = sdd_vtree_right(sdd_vtree_node);
      auto psdd_vtree_left_node = (Vtree *) sdd_vtree_data(sdd_vtree_left_node);
      auto psdd_vtree_right_node = (Vtree *) sdd_vtree_data(sdd_vtree_right_node);
      Vtree *psdd_vtree_node = new_internal_vtree(psdd_vtree_left_node, psdd_vtree_right_node);
      sdd_vtree_set_data((void *) psdd_vtree_node, sdd_vtree_node);
    }
  }
  auto psdd_vtree = (Vtree *) sdd_vtree_data(sdd_vtree);
  set_vtree_properties(psdd_vtree);
  for (Vtree *sdd_vtree_node : serialized_sdd_vtree) {
    sdd_vtree_set_data(nullptr, sdd_vtree_node);
  }
  auto unique_table = PsddUniqueTable::GetPsddUniqueTable();
  return new PsddManager(psdd_vtree, unique_table);
}
PsddManager::PsddManager(Vtree *vtree, PsddUniqueTable *unique_table)
    : vtree_(vtree), unique_table_(unique_table), node_index_(0), leaf_vtree_map_() {
  std::vector<Vtree *> serialized_vtrees = vtree_util::SerializeVtree(vtree_);
  for (Vtree *cur_v : serialized_vtrees) {
    if (sdd_vtree_is_leaf(cur_v)) {
      leaf_vtree_map_[sdd_vtree_var(cur_v)] = cur_v;
    }
  }
}
PsddManager::~PsddManager() {
  unique_table_->DeleteUnusedPsddNodes({});
  delete (unique_table_);
  sdd_vtree_free(vtree_);
}
void PsddManager::DeleteUnusedPsddNodes(const std::vector<PsddNode *> &used_nodes) {
  unique_table_->DeleteUnusedPsddNodes(used_nodes);
}

PsddNode *PsddManager::ConvertSddToPsdd(SddNode *root_node,
                                        Vtree *sdd_vtree,
                                        uintmax_t flag_index,
                                        const std::unordered_map<uint32_t, uint32_t> &variable_mapping) {
  if (sdd_node_is_false(root_node)) {
    // nullptr for PsddNode means false
    return nullptr;
  }
  if (sdd_node_is_true(root_node)) {
    return GetTrueNode(vtree_, flag_index);
  }
  std::vector<Vtree *> serialized_psdd_vtrees = vtree_util::SerializeVtree(vtree_);
  std::vector<Vtree *> serialized_sdd_vtrees = vtree_util::SerializeVtree(sdd_vtree);
  TagSddVtreeWithPsddVtree(serialized_sdd_vtrees, serialized_psdd_vtrees, variable_mapping);
  std::unordered_map<SddLiteral, PsddNode *> true_nodes_map;
  SddSize node_size = 0;
  SddNode **serialized_sdd_nodes = sdd_topological_sort(root_node, &node_size);
  std::unordered_map<SddSize, PsddNode *> node_map;
  for (auto i = 0; i < node_size; ++i) {
    SddNode *cur_node = serialized_sdd_nodes[i];
    if (sdd_node_is_decision(cur_node)) {
      Vtree *old_vtree_node = sdd_vtree_of(cur_node);
      auto new_vtree_node = (Vtree *) sdd_vtree_data(old_vtree_node);
      std::vector<PsddNode *> primes;
      std::vector<PsddNode *> subs;
      SddNode **elements = sdd_node_elements(cur_node);
      SddSize element_size = sdd_node_size(cur_node);
      std::vector<PsddParameter> parameters(element_size, PsddParameter::CreateFromDecimal(1));
      for (auto j = 0; j < element_size; j++) {
        SddNode *cur_prime = elements[2 * j];
        SddNode *cur_sub = elements[2 * j + 1];
        auto node_map_it = node_map.find(sdd_id(cur_prime));
        assert(node_map_it != node_map.end());
        PsddNode *cur_psdd_prime = node_map_it->second;
        if (sdd_node_is_true(cur_sub)) {
          PsddNode *cur_normed_psdd_prime = NormalizePsddNode(sdd_vtree_left(new_vtree_node),
                                                              cur_psdd_prime,
                                                              flag_index,
                                                              &true_nodes_map);
          PsddNode *cur_normed_psdd_sub =
              GetTrueNode(sdd_vtree_right(new_vtree_node), flag_index, &true_nodes_map);
          primes.push_back(cur_normed_psdd_prime);
          subs.push_back(cur_normed_psdd_sub);
        } else if (sdd_node_is_false(cur_sub)) {
          continue;
        } else {
          // a literal or decision
          auto node_map_sub_it = node_map.find(sdd_id(cur_sub));
          assert(node_map_sub_it != node_map.end());
          PsddNode *cur_psdd_sub = node_map_sub_it->second;
          PsddNode *cur_normed_psdd_prime = NormalizePsddNode(sdd_vtree_left(new_vtree_node),
                                                              cur_psdd_prime,
                                                              flag_index,
                                                              &true_nodes_map);
          PsddNode *cur_normed_psdd_sub = NormalizePsddNode(sdd_vtree_right(new_vtree_node),
                                                            cur_psdd_sub,
                                                            flag_index,
                                                            &true_nodes_map);
          primes.push_back(cur_normed_psdd_prime);
          subs.push_back(cur_normed_psdd_sub);
        }
      }
      assert(!primes.empty());
      PsddNode *new_decn_node = new PsddDecisionNode(node_index_, new_vtree_node, flag_index, primes, subs, parameters);
      new_decn_node = unique_table_->GetUniqueNode(new_decn_node, &node_index_);
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
      PsddNode *new_literal_node = new PsddLiteralNode(node_index_, new_vtree_node, flag_index, new_literal);
      new_literal_node = unique_table_->GetUniqueNode(new_literal_node, &node_index_);
      node_map[sdd_id(cur_node)] = new_literal_node;
    } else {
      //True false node
      continue;
    }
  }
  assert(node_map.find(sdd_id(root_node)) != node_map.end());
  PsddNode *psdd_root_node = node_map[sdd_id(root_node)];
  PsddNode *psdd_root_normalized_node =
      NormalizePsddNode(vtree_, psdd_root_node, flag_index, &true_nodes_map);
  for (auto sdd_vtree_node: serialized_sdd_vtrees) {
    sdd_vtree_set_data(nullptr, sdd_vtree_node);
  }
  free(serialized_sdd_nodes);
  return psdd_root_normalized_node;
}

PsddNode *PsddManager::GetTrueNode(Vtree *target_vtree_node,
                                   uintmax_t flag_index,
                                   std::unordered_map<SddLiteral, PsddNode *> *true_node_map) {
  if (true_node_map->find(sdd_vtree_position(target_vtree_node)) != true_node_map->end()) {
    return true_node_map->find(sdd_vtree_position(target_vtree_node))->second;
  } else {
    std::vector<Vtree *> post_order_vtree_nodes = vtree_util::SerializeVtree(target_vtree_node);
    for (auto it = post_order_vtree_nodes.rbegin(); it != post_order_vtree_nodes.rend(); it++) {
      Vtree *cur_vtree_node = *it;
      if (sdd_vtree_is_leaf(cur_vtree_node)) {
        PsddNode *new_true_node = GetPsddTopNode((uint32_t) sdd_vtree_var(cur_vtree_node),
                                                 flag_index,
                                                 PsddParameter::CreateFromDecimal(1),
                                                 PsddParameter::CreateFromDecimal(1));
        true_node_map->insert(std::make_pair(sdd_vtree_position(cur_vtree_node), new_true_node));
      } else {
        Vtree *cur_left_node = sdd_vtree_left(cur_vtree_node);
        Vtree *cur_right_node = sdd_vtree_right(cur_vtree_node);
        assert(true_node_map->find(sdd_vtree_position(cur_left_node)) != true_node_map->end());
        assert(true_node_map->find(sdd_vtree_position(cur_right_node)) != true_node_map->end());
        PsddNode *left_true_node = true_node_map->find(sdd_vtree_position(cur_left_node))->second;
        PsddNode *right_true_node = true_node_map->find(sdd_vtree_position(cur_right_node))->second;
        PsddNode *new_true_node = GetConformedPsddDecisionNode({left_true_node},
                                                               {right_true_node},
                                                               {PsddParameter::CreateFromDecimal(1)},
                                                               flag_index);
        true_node_map->insert(std::make_pair(sdd_vtree_position(cur_vtree_node), new_true_node));
      }
    }
    assert(true_node_map->find(sdd_vtree_position(target_vtree_node)) != true_node_map->end());
    return true_node_map->find(sdd_vtree_position(target_vtree_node))->second;
  }
}
PsddNode *PsddManager::GetTrueNode(Vtree *target_vtree_node, uintmax_t flag_index) {
  std::unordered_map<SddLiteral, PsddNode *> true_node_map;
  return GetTrueNode(target_vtree_node, flag_index, &true_node_map);
}

PsddNode *PsddManager::NormalizePsddNode(Vtree *target_vtree_node,
                                         PsddNode *target_psdd_node,
                                         uintmax_t flag_index,
                                         std::unordered_map<SddLiteral, PsddNode *> *true_node_map) {
  PsddNode *cur_node = target_psdd_node;
  while (cur_node->vtree_node() != target_vtree_node) {
    Vtree *cur_vtree_node = cur_node->vtree_node();
    Vtree *cur_vtree_parent_node = sdd_vtree_parent(cur_vtree_node);
    assert(cur_vtree_parent_node != nullptr);
    if (sdd_vtree_left(cur_vtree_parent_node) == cur_vtree_node) {
      auto true_node =
          GetTrueNode(sdd_vtree_right(cur_vtree_parent_node), flag_index, true_node_map);
      PsddNode *next_node =
          new PsddDecisionNode(node_index_,
                               cur_vtree_parent_node,
                               flag_index,
                               {cur_node},
                               {true_node},
                               {PsddParameter::CreateFromDecimal(1)});
      next_node = unique_table_->GetUniqueNode(next_node, &node_index_);
      cur_node = next_node;
    } else {
      assert(sdd_vtree_right(cur_vtree_parent_node) == cur_vtree_node);
      auto true_node =
          GetTrueNode(sdd_vtree_left(cur_vtree_parent_node), flag_index, true_node_map);
      PsddNode *next_node =
          new PsddDecisionNode(node_index_,
                               cur_vtree_parent_node,
                               flag_index,
                               {true_node},
                               {cur_node},
                               {PsddParameter::CreateFromDecimal(1)});
      next_node = unique_table_->GetUniqueNode(next_node, &node_index_);
      cur_node = next_node;
    }
  }
  return cur_node;
}
Vtree *PsddManager::vtree() const {
  return vtree_;
}
PsddManager *PsddManager::GetPsddManagerFromVtree(Vtree *psdd_vtree) {
  Vtree *copy_vtree = vtree_util::CopyVtree(psdd_vtree);
  auto *unique_table = PsddUniqueTable::GetPsddUniqueTable();
  return new PsddManager(copy_vtree, unique_table);
}
PsddTopNode *PsddManager::GetPsddTopNode(uint32_t variable_index,
                                         uintmax_t flag_index,
                                         const PsddParameter &positive_parameter,
                                         const PsddParameter &negative_parameter) {
  assert(leaf_vtree_map_.find(variable_index) != leaf_vtree_map_.end());
  Vtree *target_vtree_node = leaf_vtree_map_[variable_index];
  assert(sdd_vtree_is_leaf(target_vtree_node));
  auto next_node = new PsddTopNode(node_index_,
                                   target_vtree_node,
                                   flag_index,
                                   (uint32_t) sdd_vtree_var(target_vtree_node),
                                   positive_parameter,
                                   negative_parameter);
  next_node = (PsddTopNode *) unique_table_->GetUniqueNode(next_node, &node_index_);
  return next_node;
}
PsddLiteralNode *PsddManager::GetPsddLiteralNode(int32_t literal, uintmax_t flag_index) {
  assert(leaf_vtree_map_.find(abs(literal)) != leaf_vtree_map_.end());
  Vtree *target_vtree_node = leaf_vtree_map_[abs(literal)];
  assert(sdd_vtree_is_leaf(target_vtree_node));
  auto next_node = new PsddLiteralNode(node_index_, target_vtree_node, flag_index, literal);
  next_node = (PsddLiteralNode *) unique_table_->GetUniqueNode(next_node, &node_index_);
  return next_node;
}
PsddDecisionNode *PsddManager::GetConformedPsddDecisionNode(const std::vector<PsddNode *> &primes,
                                                            const std::vector<PsddNode *> &subs,
                                                            const std::vector<PsddParameter> &params,
                                                            uintmax_t flag_index) {
  std::unordered_map<SddLiteral, PsddNode *> true_node_map;
  Vtree *lca = sdd_vtree_lca(primes[0]->vtree_node(), subs[0]->vtree_node(), vtree_);
  assert(lca != nullptr);
  Vtree *left_child = sdd_vtree_left(lca);
  Vtree *right_child = sdd_vtree_right(lca);
  auto element_size = primes.size();
  std::vector<PsddNode *> conformed_primes;
  std::vector<PsddNode *> conformed_subs;
  for (auto i = 0; i < element_size; ++i) {
    PsddNode *cur_prime = primes[i];
    PsddNode *cur_sub = subs[i];
    PsddNode *cur_conformed_prime = NormalizePsddNode(left_child, cur_prime, flag_index, &true_node_map);
    PsddNode *cur_conformed_sub = NormalizePsddNode(right_child, cur_sub, flag_index, &true_node_map);
    conformed_primes.push_back(cur_conformed_prime);
    conformed_subs.push_back(cur_conformed_sub);
  }
  auto next_decn_node = new PsddDecisionNode(node_index_, lca, flag_index, conformed_primes, conformed_subs, params);
  next_decn_node = (PsddDecisionNode *) unique_table_->GetUniqueNode(next_decn_node, &node_index_);
  return next_decn_node;
}

PsddNode *PsddManager::LoadPsddNode(Vtree *target_vtree, PsddNode *root_psdd_node, uintmax_t flag_index) {
  std::vector<PsddNode *> serialized_nodes = psdd_node_util::SerializePsddNodes(root_psdd_node);
  for (auto it = serialized_nodes.rbegin(); it != serialized_nodes.rend(); ++it) {
    PsddNode *cur_node = *it;
    if (cur_node->node_type() == LITERAL_NODE_TYPE) {
      PsddLiteralNode *cur_literal_node = cur_node->psdd_literal_node();
      PsddNode *new_node = GetPsddLiteralNode(cur_literal_node->literal(), flag_index);
      cur_literal_node->SetUserData((uintmax_t) new_node);
    } else if (cur_node->node_type() == TOP_NODE_TYPE) {
      PsddTopNode *cur_top_node = cur_node->psdd_top_node();
      PsddTopNode *new_top_node = GetPsddTopNode(cur_top_node->variable_index(),
                                                 flag_index,
                                                 cur_top_node->true_parameter(),
                                                 cur_top_node->false_parameter());
      cur_top_node->SetUserData((uintmax_t) new_top_node);
    } else {
      assert(cur_node->node_type() == DECISION_NODE_TYPE);
      PsddDecisionNode *cur_decision_node = cur_node->psdd_decision_node();
      const auto &cur_primes = cur_decision_node->primes();
      const auto &cur_subs = cur_decision_node->subs();
      const auto &cur_parameters = cur_decision_node->parameters();
      std::vector<PsddNode *> new_primes(cur_primes.size(), nullptr);
      std::vector<PsddNode *> new_subs(cur_subs.size(), nullptr);
      for (auto i = 0; i < cur_primes.size(); ++i) {
        new_primes[i] = (PsddNode *) cur_primes[i]->user_data();
        new_subs[i] = (PsddNode *) cur_subs[i]->user_data();
      }
      PsddDecisionNode
          *new_decision_node = GetConformedPsddDecisionNode(new_primes, new_subs, cur_parameters, flag_index);
      cur_decision_node->SetUserData((uintmax_t) new_decision_node);
    }
  }
  auto new_root_node = (PsddNode *) root_psdd_node->user_data();
  std::unordered_map<SddLiteral, PsddNode *> true_node_map;
  PsddNode *result_node = NormalizePsddNode(target_vtree, new_root_node, flag_index, &true_node_map);
  for (PsddNode *cur_node : serialized_nodes) {
    cur_node->SetUserData(0);
  }
  return result_node;
}
PsddNode *PsddManager::NormalizePsddNode(Vtree *target_vtree_node, PsddNode *target_psdd_node, uintmax_t flag_index) {
  std::unordered_map<SddLiteral, PsddNode *> true_node_map;
  return NormalizePsddNode(target_vtree_node, target_psdd_node, flag_index, &true_node_map);
}

std::pair<PsddNode *, PsddParameter> PsddManager::Multiply(PsddNode *arg1, PsddNode *arg2, uintmax_t flag_index) {
  ComputationCache cache((uint32_t) leaf_vtree_map_.size());
  return MultiplyWithCache(arg1, arg2, this, flag_index, &cache);
}

PsddNode *PsddManager::ReadPsddFile(const char *psdd_filename, uintmax_t flag_index) {
  std::ifstream psdd_file;
  std::unordered_map<uintmax_t, PsddNode *> construct_cache;
  psdd_file.open(psdd_filename);
  if (!psdd_file) {
    std::cerr << "File " << psdd_filename << " cannot be open.";
    exit(1); // terminate with error
  }
  std::string line;
  PsddNode *root_node = nullptr;
  while (std::getline(psdd_file, line)) {
    if (line[0] == 'c') {
      continue;
    }
    if (line[0] == 'p') {
      continue;
    }
    if (line[0] == 'L') {
      std::istringstream iss(line.substr(1, std::string::npos));
      uintmax_t node_index;
      uint32_t vtree_index;
      int32_t literal;
      iss >> node_index >> vtree_index >> literal;
      PsddNode *cur_node = GetPsddLiteralNode(literal, flag_index);
      construct_cache[node_index] = cur_node;
      root_node = cur_node;
    } else if (line[0] == 'T') {
      std::istringstream iss(line.substr(1, std::string::npos));
      uintmax_t node_index;
      uint32_t vtree_index;
      uint32_t variable_index;
      double neg_log_pr;
      double pos_log_pr;
      iss >> node_index >> vtree_index >> variable_index >> neg_log_pr >> pos_log_pr;
      PsddNode *cur_node = GetPsddTopNode(variable_index,
                                          flag_index,
                                          PsddParameter::CreateFromLog(pos_log_pr),
                                          PsddParameter::CreateFromLog(neg_log_pr));
      construct_cache[node_index] = cur_node;
      root_node = cur_node;
    } else {
      assert(line[0] == 'D');
      std::istringstream iss(line.substr(1, std::string::npos));
      uintmax_t node_index;
      int vtree_index;
      uintmax_t element_size;
      iss >> node_index >> vtree_index >> element_size;
      std::vector<PsddNode *> primes;
      std::vector<PsddNode *> subs;
      std::vector<PsddParameter> params;
      for (auto j = 0; j < element_size; j++) {
        uintmax_t prime_index;
        uintmax_t sub_index;
        double weight_in_log;
        iss >> prime_index >> sub_index >> weight_in_log;
        assert(construct_cache.find(prime_index) != construct_cache.end());
        assert(construct_cache.find(sub_index) != construct_cache.end());
        PsddNode *prime_node = construct_cache[prime_index];
        PsddNode *sub_node = construct_cache[sub_index];
        primes.push_back(prime_node);
        subs.push_back(sub_node);
        params.push_back(PsddParameter::CreateFromLog(weight_in_log));
      }
      PsddNode *cur_node = GetConformedPsddDecisionNode(primes, subs, params, flag_index);
      construct_cache[node_index] = cur_node;
      root_node = cur_node;
    }
  }
  psdd_file.close();
  return root_node;
}
std::vector<PsddNode *> PsddManager::SampleParametersForMultiplePsdds(RandomDoubleGenerator *generator,
                                                                      const std::vector<PsddNode *> &root_psdd_nodes,
                                                                      uintmax_t flag_index) {
  std::vector<PsddNode *> serialized_psdd_nodes = psdd_node_util::SerializePsddNodes(root_psdd_nodes);
  for (auto node_it = serialized_psdd_nodes.rbegin(); node_it != serialized_psdd_nodes.rend(); ++node_it) {
    PsddNode *cur_node = *node_it;
    if (cur_node->node_type() == LITERAL_NODE_TYPE) {
      PsddNode *new_node = GetPsddLiteralNode(cur_node->psdd_literal_node()->literal(), flag_index);
      cur_node->SetUserData((uintmax_t) new_node);
    } else if (cur_node->node_type() == TOP_NODE_TYPE) {
      double pos_num = generator->generate();
      double neg_num = generator->generate();
      double sum = pos_num + neg_num;
      auto true_parameter = PsddParameter::CreateFromDecimal(pos_num / sum);
      auto false_parameter = PsddParameter::CreateFromDecimal(neg_num / sum);
      double sum_lg = (true_parameter + false_parameter).parameter();
      assert(std::abs(sum_lg) <= 0.0001);
      PsddNode *new_node =
          GetPsddTopNode(cur_node->psdd_top_node()->variable_index(), flag_index, true_parameter, false_parameter);
      cur_node->SetUserData((uintmax_t) new_node);
    } else {
      assert(cur_node->node_type() == DECISION_NODE_TYPE);
      PsddDecisionNode *cur_decn_node = cur_node->psdd_decision_node();
      const auto &primes = cur_decn_node->primes();
      const auto &subs = cur_decn_node->subs();
      auto element_size = primes.size();
      std::vector<PsddNode *> next_primes(element_size, nullptr);
      std::vector<PsddNode *> next_subs(element_size, nullptr);
      std::vector<PsddParameter> sampled_number(element_size);
      PsddParameter sum = PsddParameter::CreateFromDecimal(0);
      for (auto i = 0; i < element_size; ++i) {
        double cur_num = generator->generate();
        sampled_number[i] = PsddParameter::CreateFromDecimal(cur_num);
        sum = sum + sampled_number[i];
        next_primes[i] = (PsddNode *) primes[i]->user_data();
        next_subs[i] = (PsddNode *) subs[i]->user_data();
      }
      std::vector<PsddParameter> next_parameters(element_size);
      for (auto i = 0; i < element_size; ++i) {
        next_parameters[i] = sampled_number[i] / sum;
      }
      PsddNode *new_node = GetConformedPsddDecisionNode(next_primes, next_subs, next_parameters, flag_index);
      cur_node->SetUserData((uintmax_t) new_node);
    }
  }
  auto root_psdd_nodes_size = root_psdd_nodes.size();
  std::vector<PsddNode *> new_root_nodes(root_psdd_nodes_size, nullptr);
  for (auto i = 0; i < root_psdd_nodes_size; ++i) {
    new_root_nodes[i] = (PsddNode *) root_psdd_nodes[i]->user_data();
  }
  for (PsddNode *cur_node : serialized_psdd_nodes) {
    cur_node->SetUserData(0);
  }
  return new_root_nodes;
}
PsddNode *PsddManager::SampleParameters(RandomDoubleGenerator *generator,
                                        PsddNode *target_root_node,
                                        uintmax_t flag_index) {
  return SampleParametersForMultiplePsdds(generator, {target_root_node}, flag_index)[0];
}

