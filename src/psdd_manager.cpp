//
// Created by Yujia Shen on 3/20/18.
//

#include <queue>
#include <stack>
#include <cassert>
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

}

PsddManager *PsddManager::GetPsddManager(Vtree *sdd_vtree,
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
    : vtree_(vtree), unique_table_(unique_table), node_index_(0) {}
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
  std::vector<Vtree *> serialized_psdd_vtrees = vtree_util::SerializeVtree(vtree_);
  std::vector<Vtree *> serialized_sdd_vtrees = vtree_util::SerializeVtree(sdd_vtree);
  PsddNode *result_node = nullptr;
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
      PsddNode *new_decn_node = new PsddDecisionNode(node_index_, new_vtree_node, flag_index, primes, subs, {});
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
      assert(true_node_map->find(sdd_vtree_position(cur_vtree_node)) == true_node_map->end());
      if (sdd_vtree_is_leaf(cur_vtree_node)) {
        PsddNode *new_true_node = new PsddTopNode(node_index_,
                                                  cur_vtree_node,
                                                  flag_index,
                                                  static_cast<uint32_t>(sdd_vtree_var(cur_vtree_node)),
                                                  PsddParameter::CreateFromDecimal(0),
                                                  PsddParameter::CreateFromDecimal(0));
        new_true_node = unique_table_->GetUniqueNode(new_true_node, &node_index_);
        true_node_map->insert(std::make_pair(sdd_vtree_position(cur_vtree_node), new_true_node));
      } else {
        Vtree *cur_left_node = sdd_vtree_left(cur_vtree_node);
        Vtree *cur_right_node = sdd_vtree_right(cur_vtree_node);
        assert(true_node_map->find(sdd_vtree_position(cur_left_node)) != true_node_map->end());
        assert(true_node_map->find(sdd_vtree_position(cur_right_node)) != true_node_map->end());
        PsddNode *left_true_node = true_node_map->find(sdd_vtree_position(cur_left_node))->second;
        PsddNode *right_true_node = true_node_map->find(sdd_vtree_position(cur_right_node))->second;
        PsddNode *new_true_node =
            new PsddDecisionNode(node_index_, cur_vtree_node, flag_index, {left_true_node}, {right_true_node}, {});
        new_true_node = unique_table_->GetUniqueNode(new_true_node, &node_index_);
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
          new PsddDecisionNode(node_index_, cur_vtree_parent_node, flag_index, {cur_node}, {true_node}, {});
      next_node = unique_table_->GetUniqueNode(next_node, &node_index_);
      cur_node = next_node;
    } else {
      assert(sdd_vtree_right(cur_vtree_parent_node) == cur_vtree_node);
      auto true_node =
          GetTrueNode(sdd_vtree_left(cur_vtree_parent_node), flag_index, true_node_map);
      PsddNode *next_node =
          new PsddDecisionNode(node_index_, cur_vtree_parent_node, flag_index, {true_node}, {cur_node}, {});
      next_node = unique_table_->GetUniqueNode(next_node, &node_index_);
      cur_node = next_node;
    }
  }
  return cur_node;
}
Vtree *PsddManager::vtree() const {
  return vtree_;
}
