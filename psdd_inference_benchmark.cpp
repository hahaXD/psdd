//
// Created by Jason Shen on 4/22/18.
//

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <random>
#include <vector>

#include "psdd/cnf.h"
#include "psdd/optionparser.h"
#include "psdd/psdd_node.h"
#include "psdd/psdd_parameter.h"
extern "C" {
#include <sdd/sddapi.h>
}

namespace {
void add_batch_vector(const std::vector<Probability> &a,
                      const std::vector<Probability> &b,
                      std::vector<Probability> &output) {
  const size_t batch_size = a.size();
  for (auto i = 0; i < batch_size; ++i) {
    output[i] = a[i] + b[i];
  }
}

void multiply_batch_vector(const std::vector<Probability> &a,
                           const std::vector<Probability> &b,
                           std::vector<Probability> &output) {
  const size_t batch_size = a.size();
  for (auto i = 0; i < batch_size; ++i) {
    output[i] = a[i] * b[i];
  }
}

void max_batch_vector(const std::vector<Probability> &a,
                      const std::vector<Probability> &b,
                      std::vector<Probability> &output) {
  const size_t batch_size = a.size();
  for (auto i = 0; i < batch_size; ++i) {
    if (a[i] < b[i])
      output[i] = b[i];
    else
      output[i] = a[i];
  }
}

std::vector<Probability> MpepQuery(
    const std::vector<PsddNode *> &cbp_serialized_nodes,
    const std::vector<SddLiteral> &evid) {
  for (PsddNode *cur_node : cbp_serialized_nodes) {
    cur_node->SetUserData((uintmax_t) new std::vector<Probability>(
        evid.size(), Probability::CreateFromDecimal(0)));
  }
  const auto batch_size = evid.size();
  auto start = std::chrono::steady_clock::now();
  for (PsddNode *cur_node : cbp_serialized_nodes) {
    std::vector<Probability> &node_values =
        *(std::vector<Probability> *)cur_node->user_data();
    if (cur_node->node_type() == LITERAL_NODE_TYPE) {
      PsddLiteralNode *cur_literal_node = cur_node->psdd_literal_node();
      for (auto i = 0; i < batch_size; ++i) {
        if (std::abs(cur_literal_node->literal()) == std::abs(evid[i])) {
          if (cur_literal_node->literal() == evid[i]) {
            node_values[i] = Probability::CreateFromDecimal(1);
          } else {
            node_values[i] = Probability::CreateFromDecimal(0);
          }
        } else {
          node_values[i] = Probability::CreateFromDecimal(1);
        }
      }
    } else if (cur_node->node_type() == DECISION_NODE_TYPE) {
      PsddDecisionNode *cur_decn_node = cur_node->psdd_decision_node();
      const size_t element_size = cur_decn_node->primes().size();
      std::vector<Probability> batch_buffer(batch_size,
                                            Probability::CreateFromDecimal(0));
      for (auto i = 0; i < element_size; ++i) {
        auto p_v =
            (std::vector<Probability> *)cur_decn_node->primes()[i]->user_data();
        auto s_v =
            (std::vector<Probability> *)cur_decn_node->subs()[i]->user_data();
        multiply_batch_vector(*p_v, *s_v, batch_buffer);
        max_batch_vector(batch_buffer, node_values, node_values);
      }
    } else {
      assert(cur_node->node_type() == TOP_NODE_TYPE);
      PsddTopNode *cur_top_node = cur_node->psdd_top_node();
      for (auto i = 0; i < batch_size; ++i) {
        if (cur_top_node->variable_index() == std::abs(evid[i])) {
          if (evid[i] > 0) {
            node_values[i] = cur_top_node->true_parameter();
          } else {
            node_values[i] = cur_top_node->false_parameter();
          }
        } else {
          if (cur_top_node->true_parameter() >
              cur_top_node->false_parameter()) {
            node_values[i] = cur_top_node->true_parameter();
          } else {
            node_values[i] = cur_top_node->false_parameter();
          }
        }
      }
    }
  }
  auto end = std::chrono::steady_clock::now();
  auto final_result_batch =
      (std::vector<Probability> *)cbp_serialized_nodes.back()->user_data();
  std::vector<Probability> result = std::move(*final_result_batch);
  for (PsddNode *cur_node : cbp_serialized_nodes) {
    auto to_delete = (std::vector<Probability> *)cur_node->user_data();
    delete (to_delete);
  }
  std::cout << "MpepQueryTime: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                     start)
                   .count()
            << std::endl;
  return result;
}

std::vector<Probability> MarQuery(
    const std::vector<PsddNode *> &cbp_serialized_nodes,
    const std::vector<SddLiteral> &evid) {
  for (PsddNode *cur_node : cbp_serialized_nodes) {
    cur_node->SetUserData((uintmax_t) new std::vector<Probability>(
        evid.size(), Probability::CreateFromDecimal(0)));
  }
  const auto batch_size = evid.size();
  auto start = std::chrono::steady_clock::now();
  for (PsddNode *cur_node : cbp_serialized_nodes) {
    std::vector<Probability> &node_values =
        *(std::vector<Probability> *)cur_node->user_data();
    if (cur_node->node_type() == LITERAL_NODE_TYPE) {
      PsddLiteralNode *cur_literal_node = cur_node->psdd_literal_node();
      for (auto i = 0; i < batch_size; ++i) {
        if (std::abs(cur_literal_node->literal()) == std::abs(evid[i])) {
          if (cur_literal_node->literal() == evid[i]) {
            node_values[i] = Probability::CreateFromDecimal(1);
          } else {
            node_values[i] = Probability::CreateFromDecimal(0);
          }
        } else {
          node_values[i] = Probability::CreateFromDecimal(1);
        }
      }
    } else if (cur_node->node_type() == DECISION_NODE_TYPE) {
      PsddDecisionNode *cur_decn_node = cur_node->psdd_decision_node();
      const size_t element_size = cur_decn_node->primes().size();
      std::vector<Probability> batch_buffer(batch_size,
                                            Probability::CreateFromDecimal(0));
      for (auto i = 0; i < element_size; ++i) {
        auto p_v =
            (std::vector<Probability> *)cur_decn_node->primes()[i]->user_data();
        auto s_v =
            (std::vector<Probability> *)cur_decn_node->subs()[i]->user_data();
        multiply_batch_vector(*p_v, *s_v, batch_buffer);
        add_batch_vector(batch_buffer, node_values, node_values);
      }
    } else {
      assert(cur_node->node_type() == TOP_NODE_TYPE);
      PsddTopNode *cur_top_node = cur_node->psdd_top_node();
      for (auto i = 0; i < batch_size; ++i) {
        if (cur_top_node->variable_index() == std::abs(evid[i])) {
          if (evid[i] > 0) {
            node_values[i] = cur_top_node->true_parameter();
          } else {
            node_values[i] = cur_top_node->false_parameter();
          }
        } else {
          node_values[i] = Probability::CreateFromDecimal(1);
        }
      }
    }
  }
  auto end = std::chrono::steady_clock::now();
  auto final_result_batch =
      (std::vector<Probability> *)cbp_serialized_nodes.back()->user_data();
  std::vector<Probability> result = std::move(*final_result_batch);
  for (PsddNode *cur_node : cbp_serialized_nodes) {
    auto to_delete = (std::vector<Probability> *)cur_node->user_data();
    delete (to_delete);
  }
  std::cout << "MarQueryTime: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                     start)
                   .count()
            << std::endl;
  return result;
}
}  // namespace

int main(int argc, const char *argv[]) {
  const char *psdd_filename = argv[1];
  const char *vtree_filename = argv[2];

  // load PSDD from files
  auto load_psdd_start = std::chrono::steady_clock::now();
  Vtree *psdd_vtree = sdd_vtree_read(vtree_filename);
  PsddManager *psdd_manager = PsddManager::GetPsddManagerFromVtree(psdd_vtree);
  sdd_vtree_free(psdd_vtree);
  PsddNode *result_node = psdd_manager->ReadPsddFile(psdd_filename, 0);

  auto cbp_serialized_psdds = psdd_node_util::SerializePsddNodes(result_node);
  std::reverse(cbp_serialized_psdds.begin(), cbp_serialized_psdds.end());
  auto load_psdd_end = std::chrono::steady_clock::now();

  std::cout << "Psdd file is loaded with time :"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   load_psdd_end - load_psdd_start)
                   .count()
            << std::endl;
  std::cout << "PSDD size " <<  cbp_serialized_psdds.size() << std::endl;

  // generates random evids
  const size_t batch_size = 256;
  std::vector<SddLiteral> evids;
  auto mar_result = psdd_node_util::GetMarginals(cbp_serialized_psdds);
  SddLiteral var_size = mar_result.size();
  std::uniform_int_distribution<> var_sampler(1, var_size);
  std::uniform_int_distribution<> val_sampler(0, 1);
  std::random_device rd;
  std::mt19937 engine(rd());
  for (auto i = 0; i < batch_size; ++i) {
    SddLiteral var = var_sampler(engine);
    SddLiteral val = val_sampler(engine);
    if (val == 0) {
      if (mar_result[var].first != Probability::CreateFromDecimal(0)) {
        evids.push_back(-var);
      } else {
        evids.push_back(var);
      }
    } else {
      if (mar_result[var].second != Probability::CreateFromDecimal(0)) {
        evids.push_back(var);
      } else {
        evids.push_back(-var);
      }
    }
  }

  MpepQuery(cbp_serialized_psdds, evids);
  MarQuery(cbp_serialized_psdds, evids);
}
