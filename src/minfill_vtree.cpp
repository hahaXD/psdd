#include "psdd/minfill_vtree.h"

#include <assert.h>

#include <list>
#include <stack>
#include <unordered_map>
#include <vector>

#include "htd/main.hpp"

namespace {

using AdjacencyMatrix =
    std::unordered_map<SddLiteral, std::unordered_set<SddLiteral>>;

std::pair<AdjacencyMatrix, AdjacencyMatrix> ConnectedGraph(
    AdjacencyMatrix graph_adj) {
  assert(graph_adj.size() > 0);
  std::unordered_set<SddLiteral> explored_nodes;
  std::stack<SddLiteral> nodes_to_explore;
  nodes_to_explore.push(graph_adj.begin()->first);
  explored_nodes.insert(graph_adj.begin()->first);
  while (nodes_to_explore.size() > 0) {
    SddLiteral cur_node = nodes_to_explore.top();
    nodes_to_explore.pop();
    const auto &nbrs = graph_adj.find(cur_node)->second;
    for (SddLiteral cur_nbr : nbrs) {
      if (explored_nodes.find(cur_nbr) == explored_nodes.end()) {
        nodes_to_explore.push(cur_nbr);
        explored_nodes.insert(cur_nbr);
      }
    }
  }
  if (explored_nodes.size() == graph_adj.size()) {
    return std::make_pair<AdjacencyMatrix, AdjacencyMatrix>(
        std::move(graph_adj), {});
  }
  AdjacencyMatrix explored_graph;
  AdjacencyMatrix remaining_graph;
  for (const auto &adj_entry : graph_adj) {
    if (explored_nodes.find(adj_entry.first) != explored_nodes.end()) {
      explored_graph[adj_entry.first] = adj_entry.second;
    } else {
      remaining_graph[adj_entry.first] = adj_entry.second;
    }
  }
  return std::make_pair<AdjacencyMatrix, AdjacencyMatrix>(
      std::move(explored_graph), std::move(remaining_graph));
}

AdjacencyMatrix RemoveNode(AdjacencyMatrix graph_adj, SddLiteral rm_node) {
  assert(graph_adj.find(rm_node) != graph_adj.end());
  const auto &nbrs = graph_adj.find(rm_node)->second;
  for (auto nbr : nbrs) {
    auto &nbr_nbrs = graph_adj[nbr];
    assert(nbr_nbrs.find(rm_node) != nbr_nbrs.end());
    nbr_nbrs.erase(nbr_nbrs.find(rm_node));
  }
  graph_adj.erase(graph_adj.find(rm_node));
  return graph_adj;
}

Vtree *VtreeFromGraph(AdjacencyMatrix graph_adj,
                      std::list<SddLiteral> var_order) {
  if (var_order.size() == 1) {
    return new_leaf_vtree(var_order.front());
  }
  auto connected_graphs = ConnectedGraph(std::move(graph_adj));
  if (connected_graphs.second.size() == 0) {
    std::vector<SddLiteral> r_var_order;
    SddLiteral cond_var = var_order.front();
    var_order.pop_front();
    auto remaining_graph =
        RemoveNode(std::move(connected_graphs.first), cond_var);
    Vtree *left_v = new_leaf_vtree(cond_var);
    Vtree *right_v =
        VtreeFromGraph(std::move(remaining_graph), std::move(var_order));
    return new_internal_vtree(left_v, right_v);
  } else {
    std::list<SddLiteral> var_order_left;
    std::list<SddLiteral> var_order_right;
    for (auto v : var_order) {
      if (connected_graphs.first.find(v) != connected_graphs.first.end()) {
        var_order_left.push_back(v);
      } else {
        var_order_right.push_back(v);
      }
    }
    Vtree *left_v = VtreeFromGraph(std::move(connected_graphs.first),
                                   std::move(var_order_left));
    Vtree *right_v = VtreeFromGraph(std::move(connected_graphs.second),
                                    std::move(var_order_right));
    return new_internal_vtree(left_v, right_v);
  }
}

}  // namespace

MinFillVtree::MinFillVtree(UaiNetwork *network) : network_(network) {}

Vtree *MinFillVtree::ConstructVtree() {
  std::unique_ptr<htd::LibraryInstance> manager(
      htd::createManagementInstance(htd::Id::FIRST));
  htd::MultiHypergraph htd_graph(manager.get());
  for (auto i = 0; i < network_->var_size(); ++i) {
    htd_graph.addVertex();
  }
  for (const auto &cur_factor_scope : network_->factor_scopes()) {
    std::vector<htd::vertex_t> h_e;
    for (auto v : cur_factor_scope) {
      h_e.push_back(v);
    }
    htd_graph.addEdge(h_e);
  }
  htd::MinFillOrderingAlgorithm minfill_manager(manager.get());
  auto ordering = minfill_manager.computeOrdering(htd_graph);
  std::list<SddLiteral> minfill_order;
  for (auto var_index : ordering->sequence()) {
    minfill_order.push_front((SddLiteral)var_index);
  }
  delete (ordering);
  AdjacencyMatrix graph;
  for (auto i = 1; i <= network_->var_size(); ++i) {
    graph[i] = {};
  }
  for (const auto &cur_factor_scope : network_->factor_scopes()) {
    const auto cur_factor_scope_size = cur_factor_scope.size();
    for (auto vi : cur_factor_scope) {
      auto &vi_nbr = graph[vi];
      for (auto vj : cur_factor_scope) {
        if (vj != vi) {
          vi_nbr.insert(vj);
        }
      }
    }
  }
  Vtree *v = VtreeFromGraph(std::move(graph), std::move(minfill_order));
  set_vtree_properties(v);
  return v;
}
