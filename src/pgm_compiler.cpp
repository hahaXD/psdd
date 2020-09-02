
#include "psdd/pgm_compiler.h"

#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "htd/PostOrderTreeTraversal.hpp"
#include "htd/main.hpp"
#include "psdd/hypergraph_decomposition_vtree.h"
#include "psdd/jointree_vtree.h"
#include "psdd/minfill_vtree.h"
#include "psdd/psdd_manager.h"
#include "psdd/psdd_node.h"
#include "psdd/psdd_parameter.h"

extern "C" {
#include <sdd/sddapi.h>
};

PgmCompiler::PgmCompiler(std::string working_dir)
    : m_network(nullptr), m_pm(nullptr), working_dir_(std::move(working_dir)) {}

std::pair<PsddNode*, PsddParameter> PgmCompiler::compile_factor(
    size_t factor_index) {
  std::vector<size_t> factor_scope = m_network->factor_scopes()[factor_index];
  const auto& cluster_param = m_network->params()[factor_index];
  Vtree* vtree = m_pm->vtree();
  // LSB is the first variable in ordered_cluster_variables now
  std::reverse(factor_scope.begin(), factor_scope.end());
  std::vector<Vtree*> leaf_v_nodes;
  auto vtree_nodes = vtree_util::SerializeVtree(vtree);
  for (size_t var_idx : factor_scope) {
    for (Vtree* v : vtree_nodes) {
      if (sdd_vtree_is_leaf(v) && sdd_vtree_var(v) == var_idx) {
        leaf_v_nodes.push_back(v);
      }
    }
  }
  std::sort(leaf_v_nodes.begin(), leaf_v_nodes.end(),
            [](const Vtree* a, const Vtree* b) {
              return sdd_vtree_position(a) > sdd_vtree_position(b);
            });
  std::vector<size_t> var_location_in_cluster;
  for (Vtree* leaf_v : leaf_v_nodes) {
    size_t var_idx = sdd_vtree_var(leaf_v);
    for (auto j = 0; j < factor_scope.size(); ++j) {
      size_t f_var_idx = factor_scope[j];
      if (f_var_idx == var_idx) {
        var_location_in_cluster.push_back(j);
        break;
      }
    }
  }
  assert(var_location_in_cluster.size() == factor_scope.size());
  std::unordered_map<std::size_t, PsddNode*>* context_node_cache =
      new std::unordered_map<std::size_t, PsddNode*>();
  std::unordered_map<std::size_t, PsddParameter>* context_norm_cache =
      new std::unordered_map<std::size_t, PsddParameter>();
  size_t term_var_loc = var_location_in_cluster[0];
  size_t mask = (size_t)((~0x0) ^ (1 << term_var_loc));
  PsddParameter zero_param = PsddParameter::CreateFromDecimal(0);
  for (auto k = cluster_param.begin(); k != cluster_param.end(); k++) {
    size_t param_index = (size_t)(k - cluster_param.begin());
    size_t context = param_index & mask;
    if (context_node_cache->find(context) != context_node_cache->end()) {
      continue;
    } else {
      size_t neg_index = context;
      size_t pos_index = context | (1 << term_var_loc);
      PsddParameter neg_weight = cluster_param[neg_index];
      PsddParameter pos_weight = cluster_param[pos_index];
      PsddParameter new_norm = pos_weight + neg_weight;
      if (new_norm == zero_param) {
        context_node_cache->insert({context, nullptr});
        context_norm_cache->insert({context, zero_param});
      } else if (pos_weight == zero_param) {
        context_node_cache->insert(
            {context, m_pm->GetPsddLiteralNode(-sdd_vtree_var(leaf_v_nodes[0]),
                                               /*flag_index*/ 0)});
        context_norm_cache->insert({context, new_norm});
      } else if (neg_weight == zero_param) {
        context_node_cache->insert(
            {context, m_pm->GetPsddLiteralNode(sdd_vtree_var(leaf_v_nodes[0]),
                                               /*flag_index*/ 0)});
        context_norm_cache->insert({context, new_norm});
      } else {
        PsddParameter pos_weight_renorm = pos_weight / new_norm;
        PsddParameter neg_weight_renorm = neg_weight / new_norm;
        context_node_cache->insert(
            {context, m_pm->GetPsddTopNode(sdd_vtree_var(leaf_v_nodes[0]),
                                           /*flag_index*/ 0, pos_weight_renorm,
                                           neg_weight_renorm)});
        context_norm_cache->insert({context, new_norm});
      }
    }
  }
  for (auto k = leaf_v_nodes.begin() + 1; k != leaf_v_nodes.end(); k++) {
    std::unordered_map<std::size_t, PsddNode*>* context_node_cache_out =
        new std::unordered_map<std::size_t, PsddNode*>();
    std::unordered_map<std::size_t, PsddParameter>* context_norm_cache_out =
        new std::unordered_map<std::size_t, PsddParameter>();
    size_t cond_var_location =
        var_location_in_cluster[k - leaf_v_nodes.begin()];
    mask = (size_t)((~0x0) ^ (1 << cond_var_location));
    for (auto p = context_node_cache->begin(); p != context_node_cache->end();
         p++) {
      size_t last_context = p->first;
      size_t next_context = last_context & mask;
      if (context_node_cache_out->find(next_context) !=
          context_node_cache_out->end()) {
        continue;
      }
      PsddParameter neg_param = context_norm_cache->at(next_context);
      PsddParameter pos_param =
          context_norm_cache->at(next_context | (1 << cond_var_location));
      PsddParameter new_norm = pos_param + neg_param;
      if (new_norm == zero_param) {
        context_node_cache_out->insert({next_context, nullptr});
        context_norm_cache_out->insert({next_context, zero_param});
      } else if (pos_param == zero_param) {
        PsddNode* neg_lit =
            m_pm->GetPsddLiteralNode(-sdd_vtree_var(*k), /*flag_index*/ 0);
        PsddNode* next_node = m_pm->GetConformedPsddDecisionNode(
            {neg_lit}, {context_node_cache->at(next_context)},
            {PsddParameter::CreateFromDecimal(1)}, /*flag_index*/ 0);
        context_node_cache_out->insert({next_context, next_node});
        context_norm_cache_out->insert({next_context, neg_param});
      } else if (neg_param == zero_param) {
        PsddNode* pos_lit =
            m_pm->GetPsddLiteralNode(sdd_vtree_var(*k), /*flag_index*/ 0);
        PsddNode* next_node = m_pm->GetConformedPsddDecisionNode(
            {pos_lit},
            {context_node_cache->at(next_context | (1 << cond_var_location))},
            {PsddParameter::CreateFromDecimal(1)}, /*flag_index*/ 0);
        context_node_cache_out->insert({next_context, next_node});
        context_norm_cache_out->insert({next_context, pos_param});
      } else {
        PsddNode* pos_lit =
            m_pm->GetPsddLiteralNode(sdd_vtree_var(*k), /*flag_index*/ 0);
        PsddNode* neg_lit =
            m_pm->GetPsddLiteralNode(-sdd_vtree_var(*k), /*flag_index*/ 0);
        PsddNode* next_node = m_pm->GetConformedPsddDecisionNode(
            {pos_lit, neg_lit},
            {context_node_cache->at(next_context | (1 << cond_var_location)),
             context_node_cache->at(next_context)},
            {pos_param / new_norm, neg_param / new_norm}, /*flag_index*/ 0);
        context_node_cache_out->insert({next_context, next_node});
        context_norm_cache_out->insert({next_context, new_norm});
      }
    }
    delete context_node_cache;
    delete context_norm_cache;
    context_node_cache = context_node_cache_out;
    context_norm_cache = context_norm_cache_out;
  }

  assert(context_node_cache->size() == 1);
  assert(context_node_cache->find(0) != context_node_cache->end());
  assert(context_norm_cache->size() == 1);
  assert(context_norm_cache->find(0) != context_norm_cache->end());
  PsddNode* result = context_node_cache->begin()->second;
  PsddParameter result_norm = context_norm_cache->begin()->second;
  delete context_node_cache;
  delete context_norm_cache;
  return {m_pm->NormalizePsddNode(m_pm->vtree(), result, /*flag_index*/ 0),
          result_norm};
}
std::pair<PsddNode*, PsddParameter> PgmCompiler::compile_network_with_vtree(
    size_t gc_freq) {
  std::unordered_map<SddLiteral, Vtree*> var_to_vtree;
  std::vector<Vtree*> serialized_vtree =
      vtree_util::SerializeVtree(m_pm->vtree());
  for (Vtree* v : serialized_vtree) {
    if (sdd_vtree_is_leaf(v)) {
      var_to_vtree[sdd_vtree_var(v)] = v;
    }
  }

  std::unordered_map<SddLiteral, std::vector<PsddNode*>> psdds_at_vtree;

  std::vector<PsddNode*> nodes;
  PsddParameter z = PsddParameter::CreateFromDecimal(1);
  std::cout << "Start Loading factors" << std::endl;
  auto factor_size = m_network->factor_size();
  for (auto i = 0; i < factor_size; i++) {
    auto compiled_cluster = compile_factor(i);
    nodes.push_back(compiled_cluster.first);
    z = z * compiled_cluster.second;
    Vtree* attached_vnode = nullptr;
    for (auto j : m_network->factor_scopes()[i]) {
      if (attached_vnode == nullptr) {
        attached_vnode = var_to_vtree[j];
      } else {
        if (sdd_vtree_position(var_to_vtree[j]) <
            sdd_vtree_position(attached_vnode)) {
          attached_vnode = var_to_vtree[j];
        }
      }
    }
    if (psdds_at_vtree.find(sdd_vtree_position(attached_vnode)) ==
        psdds_at_vtree.end()) {
      psdds_at_vtree[sdd_vtree_position(attached_vnode)] = {nodes.back()};
    } else {
      psdds_at_vtree[sdd_vtree_position(attached_vnode)].push_back(
          nodes.back());
    }
  }
  std::cout << "Finished Loading factors" << std::endl;
  int proc_nodes = 0;
  std::reverse(serialized_vtree.begin(), serialized_vtree.end());
  std::deque<PsddNode*> output_buffer;
  for (Vtree* v : serialized_vtree) {
    SddLiteral v_idx = sdd_vtree_position(v);
    if (psdds_at_vtree.find(v_idx) != psdds_at_vtree.end()) {
      PsddNode* total = nullptr;
      std::cout << "There are " << psdds_at_vtree[v_idx].size()
                << " number of vtrees" << std::endl;
      for (PsddNode* cur_node : psdds_at_vtree[v_idx]) {
        if (total == nullptr)
          total = cur_node;
        else {
          std::cout << "Arg1 size :"
                    << psdd_node_util::SerializePsddNodes(total).size()
                    << " Arg2 size :"
                    << psdd_node_util::SerializePsddNodes(cur_node).size()
                    << std::endl;
          auto mult_result = m_pm->Multiply(total, cur_node, /*flag_index*/ 0);
          proc_nodes++;
          std::cout << "Processed " << proc_nodes << " Remaining "
                    << nodes.size() - proc_nodes << std::endl;
          total = mult_result.first;
          z *= mult_result.second;
        }
      }
      assert(total != nullptr);
      Vtree* v_parent = sdd_vtree_parent(v);
      output_buffer.push_back(total);
    }
  }
  while (output_buffer.size() > 1) {
    PsddNode* a = output_buffer.front();
    output_buffer.pop_front();
    PsddNode* b = output_buffer.front();
    output_buffer.pop_front();
    std::cout << "Arg1 size :" << psdd_node_util::SerializePsddNodes(a).size()
              << " Arg2 size :" << psdd_node_util::SerializePsddNodes(b).size()
              << std::endl;
    proc_nodes += 1;
    std::cout << "Processed " << proc_nodes << " Remaining "
              << nodes.size() - proc_nodes << std::endl;
    auto mult_result = m_pm->Multiply(a, b, /*flag_index*/ 0);
    z *= mult_result.second;
    output_buffer.push_back(mult_result.first);
    if ((proc_nodes % gc_freq) == 0) {
      std::cout << "Start to GC" << std::endl;
      std::vector<PsddNode*> used_nodes(output_buffer.begin(),
                                        output_buffer.end());
      m_pm->DeleteUnusedPsddNodes(used_nodes);
      std::cout << "Finish GC" << std::endl;
    }
  }
  return {output_buffer.front(), z};
}

std::pair<PsddNode*, PsddParameter> PgmCompiler::compile_network(
    size_t gc_freq) {
  std::unordered_map<SddLiteral, Vtree*> var_to_vtree;
  std::vector<Vtree*> serialized_vtree =
      vtree_util::SerializeVtree(m_pm->vtree());
  for (Vtree* v : serialized_vtree) {
    if (sdd_vtree_is_leaf(v)) {
      var_to_vtree[sdd_vtree_var(v)] = v;
    }
  }
  std::vector<SddLiteral> factor_orders;
  std::vector<PsddNode*> nodes;
  PsddParameter z = PsddParameter::CreateFromDecimal(1);
  std::cout << "Start Loading factors" << std::endl;
  auto factor_size = m_network->factor_size();
  for (auto i = 0; i < factor_size; i++) {
    auto compiled_cluster = compile_factor(i);
    nodes.push_back(compiled_cluster.first);
    z = z * compiled_cluster.second;
    // set factor order
    factor_orders.push_back(serialized_vtree.size());
    for (auto j : m_network->factor_scopes()[i]) {
      factor_orders[factor_orders.size() - 1] =
          std::min(factor_orders[factor_orders.size() - 1],
                   sdd_vtree_position(var_to_vtree[j]));
    }
  }
  std::cout << "Finished Loading factors" << std::endl;
  std::vector<size_t> compilation_order;
  for (auto i = 0; i < nodes.size(); ++i) {
    compilation_order.push_back(i);
  }
  std::sort(compilation_order.begin(), compilation_order.end(),
            [&](const size_t& a, const size_t& b) {
              return factor_orders[a] > factor_orders[b];
            });
  PsddNode* final_result = nullptr;
  int proc_nodes = 0;
  for (auto n_id : compilation_order) {
    std::cout << "Adding Factor " << n_id << std::endl;
    auto cur_node = nodes[n_id];
    if (final_result == nullptr) {
      final_result = cur_node;
    } else {
      std::cout << "Arg1 size :"
                << psdd_node_util::SerializePsddNodes(final_result).size()
                << " Arg2 size :"
                << psdd_node_util::SerializePsddNodes(cur_node).size()
                << std::endl;
      auto mult_result =
          m_pm->Multiply(final_result, cur_node, /*flag_index*/ 0);
      final_result = mult_result.first;
      z *= mult_result.second;
    }
    proc_nodes++;
    std::cout << "Processed " << proc_nodes << " Remaining "
              << nodes.size() - proc_nodes << std::endl;
  }
  return {final_result, z};
}

std::pair<PsddNode*, PsddParameter> PgmCompiler::compile_network_dc(
    size_t gc_freq) {
  std::unordered_map<SddLiteral, Vtree*> var_to_vtree;
  std::vector<Vtree*> serialized_vtree =
      vtree_util::SerializeVtree(m_pm->vtree());
  for (Vtree* v : serialized_vtree) {
    if (sdd_vtree_is_leaf(v)) {
      var_to_vtree[sdd_vtree_var(v)] = v;
    }
  }
  std::vector<SddLiteral> factor_orders;
  std::vector<PsddNode*> nodes;
  PsddParameter z = PsddParameter::CreateFromDecimal(1);
  std::cout << "Start Loading factors" << std::endl;
  auto factor_size = m_network->factor_size();
  for (auto i = 0; i < factor_size; i++) {
    auto compiled_cluster = compile_factor(i);
    nodes.push_back(compiled_cluster.first);
    z = z * compiled_cluster.second;
    // set factor order
    factor_orders.push_back(serialized_vtree.size());
    for (auto j : m_network->factor_scopes()[i]) {
      factor_orders[factor_orders.size() - 1] =
          std::min(factor_orders[factor_orders.size() - 1],
                   sdd_vtree_position(var_to_vtree[j]));
    }
  }
  std::cout << "Finished Loading factors" << std::endl;
  std::vector<size_t> compilation_order;
  for (auto i = 0; i < nodes.size(); ++i) {
    compilation_order.push_back(i);
  }
  std::sort(compilation_order.begin(), compilation_order.end(),
            [&](const size_t& a, const size_t& b) {
              return factor_orders[a] > factor_orders[b];
            });
  PsddNode* final_result = nullptr;
  int proc_nodes = 0;
  std::deque<PsddNode*> nodes_to_mult;
  for (auto n_id : compilation_order) {
    nodes_to_mult.push_back(nodes[n_id]);
  }
  std::cout.width(30);
  std::cout << std::left << "Arg1 size:";
  std::cout.width(30);
  std::cout << std::left << "Arg2 size:";
  std::cout.width(15);
  std::cout << std::left << "Processed";
  std::cout.width(15);
  std::cout << std::left << "Remaining" << std::endl;
  while (nodes_to_mult.size() > 1) {
    PsddNode* a = nodes_to_mult.front();
    nodes_to_mult.pop_front();
    PsddNode* b = nodes_to_mult.front();
    nodes_to_mult.pop_front();
    std::cout << "\r";
    std::cout.width(30);
    std::cout << std::left << psdd_node_util::SerializePsddNodes(a).size();
    std::cout.width(30);
    std::cout << std::left << psdd_node_util::SerializePsddNodes(b).size();
    proc_nodes += 1;
    std::cout.width(15);
    std::cout << std::left << proc_nodes;
    std::cout.width(15);
    std::cout << std::left << nodes.size() - proc_nodes << std::flush;
    auto mult_result = m_pm->Multiply(a, b, /*flag_index*/ 0);
    z *= mult_result.second;
    nodes_to_mult.push_back(mult_result.first);
    if ((proc_nodes % gc_freq) == 0) {
      std::cout << "\rStart to GC ";
      std::vector<PsddNode*> used_nodes(nodes_to_mult.begin(),
                                        nodes_to_mult.end());
      m_pm->DeleteUnusedPsddNodes(used_nodes);
      std::cout << "Finish GC" << std::flush;
    }
  }
  std::cout << std::endl;
  return {nodes_to_mult.front(), z};
}

void PgmCompiler::init_psdd_manager_from_vtree(const char* vtree_fname) {
  Vtree* v = sdd_vtree_read(vtree_fname);
  m_pm = PsddManager::GetPsddManagerFromVtree(v);
  sdd_vtree_free(v);
  return;
}

void PgmCompiler::init_psdd_manager(char mode) {
  if (mode == VTREE_METHOD_JOINTREE) {
    auto vtree_gen = JointreeVtree(m_network);
    Vtree* v = vtree_gen.ConstructVtree();
    m_pm = PsddManager::GetPsddManagerFromVtree(v);
    sdd_vtree_free(v);
    return;
  } else if (mode == VTREE_METHOD_MINFILL) {
    auto vtree_gen = MinFillVtree(m_network);
    Vtree* v = vtree_gen.ConstructVtree();
    m_pm = PsddManager::GetPsddManagerFromVtree(v);
    sdd_vtree_free(v);
    return;
  } else {
    assert(mode == VTREE_METHOD_HYPER_FIXED_BF);
    auto vtree_gen =
        HypergraphDecompositionVtree(m_network, working_dir_.c_str());
    Vtree* v = vtree_gen.ConstructVtree();
    m_pm = PsddManager::GetPsddManagerFromVtree(v);
    sdd_vtree_free(v);
    return;
    // std::string pid_affix = std::to_string(getpid());
    // char cnf_name[100];
    // char vtree_name[100];
    // char cmd[1000];
    // cnf_name[0] = '\0';
    // vtree_name[0] = '\0';
    // cmd[0] = '\0';
    // sprintf(cnf_name, "/tmp/uai_%s.cnf", pid_affix.c_str());
    // sprintf(vtree_name, "/tmp/uai_%s.vtree", pid_affix.c_str());
    // sprintf(cmd, "./miniC2D -c %s -m %d -o %s > /dev/null 2>&1", cnf_name,
    // mode,
    //         vtree_name);

    // std::string content = "";
    // content += "p cnf " + std::to_string(m_network->var_size()) + " " +
    //            std::to_string(m_network->factor_size()) + "\n";
    // auto ordered_clusters = m_network->factor_scopes();
    // for (auto k = ordered_clusters.begin(); k != ordered_clusters.end(); k++)
    // {
    //   for (auto p = k->begin(); p != k->end(); p++) {
    //     content += std::to_string(*p) + " ";
    //   }
    //   content += "0\n";
    // }
    // std::ofstream cnf_file;
    // cnf_file.open(cnf_name);
    // cnf_file << content;
    // cnf_file.close();
    // system(cmd);
    // Vtree* v = sdd_vtree_read(vtree_name);
    // m_pm = PsddManager::GetPsddManagerFromVtree(v);
    // sdd_vtree_free(v);
    // std::remove(vtree_name);
    // std::remove(cnf_name);
    // return;
  }
}

void PgmCompiler::read_uai_file(const char* uai_file) {
  assert(m_network == nullptr);
  m_network = new UaiNetwork();
  m_network->read_file(uai_file);
}

PsddManager* PgmCompiler::psdd_manager() const { return m_pm; }
