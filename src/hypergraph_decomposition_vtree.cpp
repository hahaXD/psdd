#include "psdd/hypergraph_decomposition_vtree.h"

#include <assert.h>

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "kahypar/libkahypar.h"

namespace {
const char* kahypar_config = R"(
# general
quiet=true
mode=recursive
objective=cut
seed=-1
cmaxnet=-1
vcycles=0
# main -> preprocessing -> min hash sparsifier
p-use-sparsifier=true
p-sparsifier-min-median-he-size=28
p-sparsifier-max-hyperedge-size=1200
p-sparsifier-max-cluster-size=10
p-sparsifier-min-cluster-size=2
p-sparsifier-num-hash-func=5
p-sparsifier-combined-num-hash-func=100
# main -> preprocessing -> community detection
p-detect-communities=true
p-detect-communities-in-ip=false
p-reuse-communities=false
p-max-louvain-pass-iterations=100
p-min-eps-improvement=0.0001
p-louvain-edge-weight=hybrid
# main -> coarsening
c-type=heavy_lazy
c-s=3.25
c-t=160
# main -> coarsening -> rating
c-rating-score=heavy_edge 
c-rating-use-communities=true
c-rating-heavy_node_penalty=multiplicative
c-rating-acceptance-criterion=best
c-fixed-vertex-acceptance-criterion=free_vertex_only
# main -> initial partitioning
i-mode=direct
i-technique=flat
# initial partitioning -> initial partitioning
i-algo=pool
i-runs=20
# initial partitioning -> local search 
i-r-type=twoway_fm
i-r-runs=-1
i-r-fm-stop=simple
i-r-fm-stop-i=50
# main -> local search
r-type=twoway_fm_hyperflow_cutter
r-runs=-1
r-fm-stop=adaptive_opt
r-fm-stop-alpha=1
r-fm-stop-i=350
# local_search -> flow scheduling and heuristics
r-flow-execution-policy=exponential
# local_search -> hyperflowcutter configuration
r-hfc-size-constraint=mf-style
r-hfc-scaling=16
r-hfc-distance-based-piercing=true
r-hfc-mbc=true
)";

kahypar_context_t* CreateContext(const char* working_dir) {
  std::ofstream config_file;
  char fname[500];
  fname[0] = '\0';
  strcpy(fname, working_dir);
  strcat(fname, "/config.ini");
  config_file.open(fname);
  config_file << kahypar_config;
  config_file.close();
  kahypar_context_t* context = kahypar_context_new();
  kahypar_configure_context_from_file(context, fname);
  return context;
}

Vtree* BaseCase(const std::vector<std::vector<SddLiteral>>& factor_scopes) {
  std::unordered_set<SddLiteral> variables;
  for (const auto& factor : factor_scopes) {
    for (SddLiteral v : factor) {
      if (variables.find(v) == variables.end()) {
        variables.insert(v);
      }
    }
  }
  assert(variables.size() > 0);
  if (variables.size() == 1) {
    return new_leaf_vtree(*variables.begin());
  }
  if (factor_scopes.size() <= 3) {
    std::vector<SddLiteral> vars_vec(variables.begin(), variables.end());
    Vtree* v = new_leaf_vtree(vars_vec[0]);
    for (auto i = 1; i < vars_vec.size(); ++i) {
      v = new_internal_vtree(new_leaf_vtree(vars_vec[i]), v);
    }
    return v;
  }
  return nullptr;
}

struct HyperGraphPartitionResult {
  HyperGraphPartitionResult(HyperGraphPartitionResult&& target)
      : left_fs_(std::move(target.left_fs_)),
        right_fs_(std::move(target.right_fs_)),
        contexts_(std::move(target.contexts_)) {}
  HyperGraphPartitionResult(std::vector<std::vector<SddLiteral>> left_fs,
                            std::vector<std::vector<SddLiteral>> right_fs,
                            std::vector<SddLiteral> contexts)
      : left_fs_(std::move(left_fs)),
        right_fs_(std::move(right_fs)),
        contexts_(std::move(contexts)) {}
  std::vector<std::vector<SddLiteral>> left_fs_;
  std::vector<std::vector<SddLiteral>> right_fs_;
  std::vector<SddLiteral> contexts_;
};

HyperGraphPartitionResult PartitionGraph(
    kahypar_context_t* context,
    std::vector<std::vector<SddLiteral>> factor_scopes) {
  std::unordered_map<SddLiteral, std::vector<int>> variable_to_factor_indexes;
  size_t factor_idx = 0;
  std::vector<size_t> eptr_vec;
  eptr_vec.push_back(0);
  std::vector<kahypar_hyperedge_id_t> eind_vec;
  for (const auto& cur_factor_scope : factor_scopes) {
    for (auto v : cur_factor_scope) {
      if (variable_to_factor_indexes.find(v) ==
          variable_to_factor_indexes.end()) {
        variable_to_factor_indexes[v] = {};
      }
      variable_to_factor_indexes[v].push_back(factor_idx);
    }
    factor_idx++;
  }

  for (const auto& vtf_pair : variable_to_factor_indexes) {
    for (auto idx : vtf_pair.second) {
      eind_vec.push_back(idx);
    }
    eptr_vec.push_back(eind_vec.size());
  }

  const kahypar_hypernode_id_t num_vertices = factor_scopes.size();
  const kahypar_hyperedge_id_t num_hyperedges =
      variable_to_factor_indexes.size();
  const double imbalance = 0.3;
  const kahypar_partition_id_t k = 2;
  kahypar_hyperedge_weight_t objective = 0;

  std::vector<kahypar_partition_id_t> partition(num_vertices, -1);

  kahypar_partition(num_vertices, num_hyperedges, imbalance, k,
                    /*vertex_weights */ nullptr, /*edge_weights*/ nullptr,
                    eptr_vec.data(), eind_vec.data(), &objective, context,
                    partition.data());

  // Extract context variables
  std::unordered_set<SddLiteral> vars_left;
  std::unordered_set<SddLiteral> vars_right;
  for (auto i = 0; i < num_vertices; ++i) {
    if (partition[i] == 0) {
      for (SddLiteral v : factor_scopes[i]) {
        vars_left.insert(v);
      }
    } else {
      for (SddLiteral v : factor_scopes[i]) {
        vars_right.insert(v);
      }
    }
  }
  std::vector<SddLiteral> context_vars;
  for (SddLiteral v : vars_right) {
    if (vars_left.find(v) != vars_left.end()) {
      context_vars.push_back(v);
    }
  }

  std::unordered_set<SddLiteral> context_vars_set(context_vars.begin(),
                                                  context_vars.end());

  // Generate Left and Right Factors
  std::vector<std::vector<SddLiteral>> left_factors;
  std::vector<std::vector<SddLiteral>> right_factors;
  for (auto i = 0; i < num_vertices; ++i) {
    std::vector<SddLiteral> new_factor_scope;
    for (SddLiteral v : factor_scopes[i]) {
      if (context_vars_set.find(v) == context_vars_set.end()) {
        new_factor_scope.push_back(v);
      }
    }
    if (new_factor_scope.size() > 0) {
      if (vars_left.find(new_factor_scope[0]) != vars_left.end()) {
        left_factors.emplace_back(std::move(new_factor_scope));
      } else {
        right_factors.emplace_back(std::move(new_factor_scope));
      }
    }
  }
  return HyperGraphPartitionResult(std::move(left_factors),
                                   std::move(right_factors),
                                   std::move(context_vars));
}

Vtree* FactorGraphToVtree(kahypar_context_t* context,
                          std::vector<std::vector<SddLiteral>> factor_scopes) {
  auto hgpr = PartitionGraph(context, factor_scopes);
  Vtree* left_v = nullptr;
  Vtree* right_v = nullptr;
  if (hgpr.left_fs_.size() != 0) {
    left_v = BaseCase(hgpr.left_fs_);
    if (left_v == nullptr) left_v = FactorGraphToVtree(context, hgpr.left_fs_);
  }
  if (hgpr.right_fs_.size() != 0) {
    right_v = BaseCase(hgpr.right_fs_);
    if (right_v == nullptr)
      right_v = FactorGraphToVtree(context, hgpr.right_fs_);
  }
  Vtree* child_v = nullptr;
  if (left_v == nullptr)
    child_v = right_v;
  else if (right_v == nullptr)
    child_v = left_v;
  else
    child_v = new_internal_vtree(left_v, right_v);

  if (child_v == nullptr) {
    assert(hgpr.contexts_.size() > 0);
    Vtree* v = new_leaf_vtree(hgpr.contexts_[0]);
    for (auto i = 1; i < hgpr.contexts_.size(); ++i) {
      v = new_internal_vtree(new_leaf_vtree(hgpr.contexts_[i]), v);
    }
    return v;
  } else if (hgpr.contexts_.size() == 0) {
    return child_v;
  } else {
    for (auto i = 0; i < hgpr.contexts_.size(); ++i) {
      child_v = new_internal_vtree(new_leaf_vtree(hgpr.contexts_[i]), child_v);
    }
    return child_v;
  }
}
}  // namespace

Vtree* HypergraphDecompositionVtree::ConstructVtree() {
  auto context = CreateContext(working_dir_);
  const std::vector<std::vector<size_t>>& factor_scopes =
      network_->factor_scopes();
  std::vector<std::vector<SddLiteral>> factor_scope_cpy;
  for (const auto& cur_factor_scope : factor_scopes) {
    std::vector<SddLiteral> t(cur_factor_scope.begin(), cur_factor_scope.end());
    factor_scope_cpy.emplace_back(std::move(t));
  }
  Vtree* v = BaseCase(factor_scope_cpy);
  auto start = std::chrono::steady_clock::now();
  if (v == nullptr) {
    v = FactorGraphToVtree(context, std::move(factor_scope_cpy));
    set_vtree_properties(v);
  }
  auto end = std::chrono::steady_clock::now();
  std::cout << "Hypergraph vtree takes time "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                     start)
                   .count()
            << " ms" << std::endl;
  set_vtree_properties(v);
  kahypar_context_free(context);
  return v;
}
