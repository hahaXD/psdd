//
// Created by Jason Shen on 4/2/17.
//

#include "psdd/uai_network.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

UaiNetwork::UaiNetwork() {}

UaiNetwork::UaiNetwork(size_t var_size, size_t factor_size, int network_type,
                       std::vector<std::vector<size_t>> clusters,
                       std::vector<std::vector<PsddParameter>> params)
    : m_var_size(var_size),
      m_factor_size(factor_size),
      m_network_type(network_type),
      m_clusters(std::move(clusters)),
      m_params(std::move(params)) {}

void UaiNetwork::read_file(const char* uai_file) {
  std::ifstream uaifs(uai_file, std::ifstream::in);
  if (!uaifs) {
    std::cerr << "factor file " << uai_file << " cannot be open." << std::endl;
  }
  std::string string_buf = "";
  getline(uaifs, string_buf);
  std::string network_type = "";
  if (string_buf.find("BAYES") != std::string::npos) {
    m_network_type = BAYESIAN_NETWORK_TYPE;
  } else {
    m_network_type = MARKOV_NETWORK_TYPE;
  }
  getline(uaifs, string_buf);
  size_t var_num = (size_t)stoi(string_buf);
  m_var_size = var_num;
  getline(uaifs, string_buf);
  getline(uaifs, string_buf);
  int factor_num = stoi(string_buf);
  m_factor_size = (size_t)factor_num;
  for (int i = 0; i < m_factor_size; i++) {
    std::vector<size_t> order_list;
    getline(uaifs, string_buf);
    std::istringstream iss(string_buf);
    int var_index = 0;
    int size;
    iss >> size;
    while (iss >> var_index) {
      order_list.push_back((size_t)(var_index + 1));
    }
    m_clusters.push_back(order_list);
  }
  for (int i = 0; i < m_factor_size; i++) {
    getline(uaifs, string_buf);
    while (string_buf == "") {
      getline(uaifs, string_buf);
    }
    size_t entry_cnt;
    std::istringstream iss(string_buf);
    iss >> entry_cnt;
    double cur_entry;
    std::vector<PsddParameter> param_list;
    param_list.reserve(entry_cnt);
    while (iss >> cur_entry) {
      param_list.push_back(PsddParameter::CreateFromDecimal(cur_entry));
    }
    while (param_list.size() < entry_cnt) {
      getline(uaifs, string_buf);
      iss.str(string_buf);
      iss.clear();
      while (iss >> cur_entry) {
        param_list.push_back(PsddParameter::CreateFromDecimal(cur_entry));
      }
    }
    assert(param_list.size() == entry_cnt);
    assert(entry_cnt != 0);
    m_params.push_back(param_list);
  }
}

size_t UaiNetwork::var_size() { return m_var_size; }

size_t UaiNetwork::factor_size() { return m_factor_size; }

int UaiNetwork::network_type() const { return m_network_type; }

const std::vector<std::vector<size_t>>& UaiNetwork::factor_scopes() const {
  return m_clusters;
}

const std::vector<std::vector<PsddParameter>>& UaiNetwork::params() const {
  return m_params;
}
