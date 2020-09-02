#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <list>
#include <memory>
#include <vector>

#include "psdd/hypergraph_decomposition_vtree.h"
#include "psdd/minfill_vtree.h"
#include "psdd/psdd_node.h"
#include "psdd/psdd_parameter.h"
#include "psdd/uai_network.h"

namespace {
std::unique_ptr<UaiNetwork> DisconnectedFactorGraph(size_t var_per_side,
                                                    size_t side_size) {
  std::vector<std::vector<size_t>> clusters;
  std::vector<std::vector<PsddParameter>> params;
  for (size_t i = 0; i < side_size; ++i) {
    std::vector<SddLiteral> var_idxs;
    for (auto j = 0; j < var_per_side; ++j) {
      var_idxs.push_back((var_per_side)*i + 1 + j);
    }
    for (size_t ji : var_idxs) {
      for (size_t jj : var_idxs) {
        if (ji != jj) {
          clusters.push_back({ji, jj});
          params.push_back({PsddParameter::CreateFromDecimal(0.5),
                            PsddParameter::CreateFromDecimal(0.5),
                            PsddParameter::CreateFromDecimal(0.5),
                            PsddParameter::CreateFromDecimal(0.5)});
        }
      }
    }
  }
  return std::make_unique<UaiNetwork>(
      /*var_size*/ (var_per_side)*side_size,
      /*factor_size*/ clusters.size(),
      /*network_type*/ 1, std::move(clusters), std::move(params));
}

std::unique_ptr<UaiNetwork> RendezvousFactorGraph(size_t var_per_side,
                                                  size_t side_size) {
  std::vector<std::vector<size_t>> clusters;
  std::vector<std::vector<PsddParameter>> params;
  for (size_t i = 0; i < side_size; ++i) {
    std::vector<SddLiteral> var_idxs;
    var_idxs.push_back(1);
    for (auto j = 0; j < var_per_side - 1; ++j) {
      var_idxs.push_back((var_per_side - 1) * i + 2 + j);
    }
    for (size_t ji : var_idxs) {
      for (size_t jj : var_idxs) {
        if (ji != jj) {
          clusters.push_back({ji, jj});
          params.push_back({PsddParameter::CreateFromDecimal(0.5),
                            PsddParameter::CreateFromDecimal(0.5),
                            PsddParameter::CreateFromDecimal(0.5),
                            PsddParameter::CreateFromDecimal(0.5)});
        }
      }
    }
  }
  return std::make_unique<UaiNetwork>(
      /*var_size*/ (var_per_side - 1) * side_size + 1,
      /*factor_size*/ clusters.size(),
      /*network_type*/ 1, std::move(clusters), std::move(params));
}

std::unique_ptr<UaiNetwork> LinearFactorGraph(size_t var_size) {
  std::vector<std::vector<size_t>> clusters;
  std::vector<std::vector<PsddParameter>> params;
  for (size_t i = 1; i < var_size; ++i) {
    clusters.push_back({i, i + 1});
    params.push_back({PsddParameter::CreateFromDecimal(0.5),
                      PsddParameter::CreateFromDecimal(0.5),
                      PsddParameter::CreateFromDecimal(0.5),
                      PsddParameter::CreateFromDecimal(0.5)});
  }
  return std::make_unique<UaiNetwork>(var_size, /*factor_size*/ var_size - 1,
                                      /*network_type*/ 1, std::move(clusters),
                                      std::move(params));
}
}  // namespace

TEST(VTREE_FROM_MINFILL_TEST, DISCONNECTED_GRAPH_TEST) {
  SddLiteral num_sides = 4;
  SddLiteral var_per_side = 10;
  auto uai_network = DisconnectedFactorGraph(var_per_side, num_sides);
  auto var_size = uai_network->var_size();
  auto min_fill_vtree = MinFillVtree(uai_network.get());
  auto vtree = min_fill_vtree.ConstructVtree();
  auto serialized_vtree = vtree_util::SerializeVtree(vtree);

  std::vector<std::unordered_set<SddLiteral>> variables_per_side;
  for (auto i = 0; i < num_sides; ++i) {
    std::unordered_set<SddLiteral> var_set;
    for (auto j = 0; j < var_per_side; ++j) {
      SddLiteral var_idx = i * (var_per_side) + 1 + j;
      var_set.insert(var_idx);
    }
    variables_per_side.push_back(var_set);
  }
  std::vector<std::unordered_set<SddLiteral>> variables_under_vtree;
  Vtree *v = vtree;
  for (auto i = 0; i < num_sides - 1; ++i) {
    std::unordered_set<SddLiteral> vars;
    Vtree *cur_v = sdd_vtree_left(v);
    auto k = vtree_util::VariablesUnderVtree(cur_v);
    for (auto var : k) {
      vars.insert(var);
    }
    variables_under_vtree.push_back(vars);
    v = sdd_vtree_right(v);
  }
  std::unordered_set<SddLiteral> vars;
  for (auto var : vtree_util::VariablesUnderVtree(v)) {
    vars.insert(var);
  }
  variables_under_vtree.push_back(vars);
  for (const auto &cur_vars : variables_under_vtree) {
    bool found = false;
    for (const auto &ref_vars : variables_per_side) {
      if (ref_vars.find(*cur_vars.begin()) != ref_vars.end()) {
        found = true;
        for (auto var : cur_vars) {
          EXPECT_NE(ref_vars.find(var), ref_vars.end());
        }
        break;
      }
    }
    EXPECT_TRUE(found);
  }

  sdd_vtree_free(vtree);
}

TEST(VTREE_FROM_MINFILL_TEST, LINEAR_FACTOR_GRAPH_TEST) {
  SddLiteral var_size = 10;
  auto uai_network = LinearFactorGraph(var_size);
  auto min_fill_vtree = MinFillVtree(uai_network.get());
  auto vtree = min_fill_vtree.ConstructVtree();
  auto serialized_vtree = vtree_util::SerializeVtree(vtree);
  EXPECT_EQ(serialized_vtree.size(), var_size * 2 - 1);
  std::vector<Vtree *> vtree_leaves;
  std::function<void(Vtree *)> in_order_leaf_tr;
  in_order_leaf_tr = [&](Vtree *v) {
    if (sdd_vtree_is_leaf(v)) {
      vtree_leaves.push_back(v);
    } else {
      in_order_leaf_tr(sdd_vtree_left(v));
      in_order_leaf_tr(sdd_vtree_right(v));
    }
  };
  in_order_leaf_tr(vtree);
  ASSERT_TRUE(vtree_leaves.size() == var_size);
  std::list<SddLiteral> variables;
  for (auto i = 1; i <= var_size; ++i) {
    variables.push_back(i);
  }
  for (auto it = vtree_leaves.rbegin(); it != vtree_leaves.rend(); ++it) {
    Vtree *v = *it;
    EXPECT_TRUE(sdd_vtree_var(v) == variables.front() ||
                sdd_vtree_var(v) == variables.back());
    if (sdd_vtree_var(v) == variables.front()) {
      variables.pop_front();
    } else {
      variables.pop_back();
    }
  }
  sdd_vtree_free(vtree);
}

TEST(VTREE_FROM_HYPERGRAPH_PARTITION_TEST, RENDEZVOUS_FACTOR_GRAPH_TEST) {
  SddLiteral var_per_side = 10;
  SddLiteral side_size = 2;
  auto uai_network = RendezvousFactorGraph(var_per_side, side_size);
  auto hypergraph_vtree = HypergraphDecompositionVtree(uai_network.get(), ".");
  auto vtree = hypergraph_vtree.ConstructVtree();
  auto serialized_vtree = vtree_util::SerializeVtree(vtree);
  SddLiteral first_cond_var = 1;
  ASSERT_TRUE(sdd_vtree_is_leaf(sdd_vtree_left(vtree)));
  EXPECT_EQ(sdd_vtree_var(sdd_vtree_left(vtree)), first_cond_var);
  std::vector<SddLiteral> first_grp =
      vtree_util::VariablesUnderVtree(sdd_vtree_left(sdd_vtree_right(vtree)));
  auto first_grp_set =
      std::unordered_set<SddLiteral>(first_grp.begin(), first_grp.end());
  std::vector<SddLiteral> second_grp =
      vtree_util::VariablesUnderVtree(sdd_vtree_right(sdd_vtree_right(vtree)));
  auto second_grp_set =
      std::unordered_set<SddLiteral>(second_grp.begin(), second_grp.end());
  for (auto i = 2; i < 2 + var_per_side - 1; ++i) {
    if (first_grp_set.find(2) != first_grp_set.end()) {
      EXPECT_TRUE(first_grp_set.find(i) != first_grp_set.end());
    } else {
      EXPECT_TRUE(second_grp_set.find(i) != second_grp_set.end());
    }
  }
  for (auto i = 2 + var_per_side - 1; i < 2 + 2 * var_per_side - 2; ++i) {
    if (first_grp_set.find(2 + var_per_side - 1) != first_grp_set.end()) {
      EXPECT_TRUE(first_grp_set.find(i) != first_grp_set.end());
    } else {
      EXPECT_TRUE(second_grp_set.find(i) != second_grp_set.end());
    }
  }

  sdd_vtree_free(vtree);
}
