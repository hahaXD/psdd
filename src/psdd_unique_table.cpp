//
// Created by Yujia Shen on 10/20/17.
//

#include "psdd_unique_table.h"
#include "psdd_node.h"

#include <unordered_set>

namespace {
struct UniqueTableFunctional {
  std::size_t operator()(const PsddNode *node) const {
    return node->hash_value();
  }
  std::size_t operator()(const PsddLiteralNode *node) const {
    return node->hash_value();
  }
  std::size_t operator()(const PsddDecisionNode *node) const {
    return node->hash_value();
  }
  std::size_t operator()(const PsddTopNode *node) {
    return node->hash_value();
  }
  bool operator()(const PsddNode *node_a, const PsddNode *node_b) const {
    if (node_a->node_type() != node_b->node_type()) {
      return false;
    }
    if (node_a->node_type() == 1) {
      return *((PsddLiteralNode *) node_a) == *((PsddLiteralNode *) node_b);
    } else if (node_a->node_type() == 2) {
      return *((PsddDecisionNode *) node_a) == *((PsddDecisionNode *) node_b);
    } else {
      return *((PsddTopNode *) node_a) == *((PsddTopNode *) node_b);
    }
  }
  bool operator()(const PsddLiteralNode *node_a, const PsddLiteralNode *node_b) const {
    return *node_a == *node_b;
  }
  bool operator()(const PsddDecisionNode *node_a, const PsddDecisionNode *node_b) const {
    return *node_a == *node_b;
  }
  bool operator()(const PsddTopNode *node_a, const PsddTopNode *node_b) {
    return *node_a == *node_b;
  }
};
class PsddUniqueTableImp : public PsddUniqueTable {
 public:
  PsddUniqueTableImp() : PsddUniqueTable() {}
  ~PsddUniqueTableImp() override = default;
  PsddNode *GetUniqueNode(PsddNode *node, uintmax_t *node_index) override {
    if (node->node_type() == 1) {
      auto cur_literal_node = (PsddLiteralNode *) node;
      SddLiteral cur_node_vtree_position = sdd_vtree_position(cur_literal_node->vtree_node());
      auto literal_node_map_at_vtree = literal_node_table_.find(cur_node_vtree_position);
      if (literal_node_map_at_vtree != literal_node_table_.end()) {
        auto found_node = literal_node_map_at_vtree->second.find(cur_literal_node);
        if (found_node == literal_node_map_at_vtree->second.end()) {
          literal_node_map_at_vtree->second.insert(cur_literal_node);
          if (node_index != nullptr) {
            *node_index += 1;
          }
          return node;
        } else {
          PsddNode *found_node_ptr = *found_node;
          delete (node);
          return found_node_ptr;
        }
      } else {
        literal_node_table_[cur_node_vtree_position] =
            std::unordered_set<PsddLiteralNode *, UniqueTableFunctional, UniqueTableFunctional>({cur_literal_node});
        if (node_index != nullptr) {
          *node_index += 1;
        }
        return node;
      }
    } else if (node->node_type() == 2) {
      auto cur_decision_node = (PsddDecisionNode *) node;
      SddLiteral cur_node_vtree_position = sdd_vtree_position(cur_decision_node->vtree_node());
      auto decision_node_map_at_vtree = decision_node_table_.find(cur_node_vtree_position);
      if (decision_node_map_at_vtree != decision_node_table_.end()) {
        auto found_node = decision_node_map_at_vtree->second.find(cur_decision_node);
        if (found_node == decision_node_map_at_vtree->second.end()) {
          decision_node_map_at_vtree->second.insert(cur_decision_node);
          if (node_index != nullptr) {
            *node_index += 1;
          }
          return node;
        } else {
          PsddNode *found_node_ptr = *found_node;
          delete (node);
          return found_node_ptr;
        }
      } else {
        decision_node_table_[cur_node_vtree_position] =
            std::unordered_set<PsddDecisionNode *, UniqueTableFunctional, UniqueTableFunctional>({cur_decision_node});
        if (node_index != nullptr) {
          *node_index += 1;
        }
        return node;
      }
    } else {
      // node_type == 3
      auto cur_top_node = (PsddTopNode *) node;
      SddLiteral cur_node_vtree_position = sdd_vtree_position(cur_top_node->vtree_node());
      auto top_node_map_at_vtree = top_node_table_.find(cur_node_vtree_position);
      if (top_node_map_at_vtree != top_node_table_.end()) {
        auto found_node = top_node_map_at_vtree->second.find(cur_top_node);
        if (found_node == top_node_map_at_vtree->second.end()) {
          top_node_map_at_vtree->second.insert(cur_top_node);
          if (node_index != nullptr) {
            *node_index += 1;
          }
          return node;
        } else {
          PsddNode *found_node_ptr = *found_node;
          delete (node);
          return found_node_ptr;
        }
      } else {
        top_node_table_[cur_node_vtree_position] =
            std::unordered_set<PsddTopNode *, UniqueTableFunctional, UniqueTableFunctional>({cur_top_node});
        if (node_index != nullptr) {
          *node_index += 1;
        }
        return node;
      }
    }
  }
  void DeletePsddNodesWithoutFlagIndexes(const std::unordered_set<uintmax_t> &flag_index) override {
    // check decision map
    auto decision_table_it = decision_node_table_.begin();
    while (decision_table_it != decision_node_table_.end()) {
      auto node_it = decision_table_it->second.begin();
      while (node_it != decision_table_it->second.end()) {
        if (flag_index.find((*node_it)->flag_index()) == flag_index.end()) {
          node_it = decision_table_it->second.erase(node_it);
        } else {
          ++node_it;
        }
      }
      if (decision_table_it->second.empty()) {
        decision_table_it = decision_node_table_.erase(decision_table_it);
      } else {
        ++decision_table_it;
      }
    }
    // check literal map
    auto literal_table_it = literal_node_table_.begin();
    while (literal_table_it != literal_node_table_.end()) {
      auto node_it = literal_table_it->second.begin();
      while (node_it != literal_table_it->second.end()) {
        if (flag_index.find((*node_it)->flag_index()) == flag_index.end()) {
          node_it = literal_table_it->second.erase(node_it);
        } else {
          ++node_it;
        }
      }
      if (literal_table_it->second.empty()) {
        literal_table_it = literal_node_table_.erase(literal_table_it);
      } else {
        ++literal_table_it;
      }
    }
    // check top map
    auto top_table_it = top_node_table_.begin();
    while (top_table_it != top_node_table_.end()) {
      auto node_it = top_table_it->second.begin();
      while (node_it != top_table_it->second.end()) {
        if (flag_index.find((*node_it)->flag_index()) == flag_index.end()) {
          node_it = top_table_it->second.erase(node_it);
        } else {
          ++node_it;
        }
      }
      if (top_table_it->second.empty()) {
        top_table_it = top_node_table_.erase(top_table_it);
      } else {
        ++top_table_it;
      }
    }
  }

  // TODO:testing this function
  void DeleteUnusedPsddNodes(const std::vector<PsddNode *> &used_psdd_nodes) override {
    auto covered_nodes = psdd_node_util::GetCoveredPsddNodes(used_psdd_nodes);
    // check decision map
    auto decision_table_it = decision_node_table_.begin();
    while (decision_table_it != decision_node_table_.end()) {
      auto node_it = decision_table_it->second.begin();
      while (node_it != decision_table_it->second.end()) {
        if (covered_nodes.find((*node_it)->node_index()) == covered_nodes.end()) {
          node_it = decision_table_it->second.erase(node_it);
        } else {
          ++node_it;
        }
      }
      if (decision_table_it->second.empty()) {
        decision_table_it = decision_node_table_.erase(decision_table_it);
      } else {
        ++decision_table_it;
      }
    }
    // check literal map
    auto literal_table_it = literal_node_table_.begin();
    while (literal_table_it != literal_node_table_.end()) {
      auto node_it = literal_table_it->second.begin();
      while (node_it != literal_table_it->second.end()) {
        if (covered_nodes.find((*node_it)->node_index()) == covered_nodes.end()) {
          node_it = literal_table_it->second.erase(node_it);
        } else {
          ++node_it;
        }
      }
      if (literal_table_it->second.empty()) {
        literal_table_it = literal_node_table_.erase(literal_table_it);
      } else {
        ++literal_table_it;
      }
    }
    // check top map
    auto top_table_it = top_node_table_.begin();
    while (top_table_it != top_node_table_.end()) {
      auto node_it = top_table_it->second.begin();
      while (node_it != top_table_it->second.end()) {
        if (covered_nodes.find((*node_it)->node_index()) == covered_nodes.end()) {
          node_it = top_table_it->second.erase(node_it);
        } else {
          ++node_it;
        }
      }
      if (top_table_it->second.empty()) {
        top_table_it = top_node_table_.erase(top_table_it);
      } else {
        ++top_table_it;
      }
    }
  }
 private:
  std::unordered_map<SddLiteral, std::unordered_set<PsddDecisionNode *, UniqueTableFunctional, UniqueTableFunctional>>
      decision_node_table_;
  std::unordered_map<SddLiteral, std::unordered_set<PsddLiteralNode *, UniqueTableFunctional, UniqueTableFunctional>>
      literal_node_table_;
  std::unordered_map<SddLiteral, std::unordered_set<PsddTopNode *, UniqueTableFunctional, UniqueTableFunctional>>
      top_node_table_;
};
}

PsddUniqueTable *PsddUniqueTable::GetPsddUniqueTable() {
  return new PsddUniqueTableImp();
}