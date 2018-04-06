//
// Created by Yujia Shen on 10/29/17.
//
#include <cmath>
#include <fstream>
#include <assert.h>
#include <iostream>
#include "csvparser.h"
#include "binary_data.h"
#include <json.hpp>

using nlohmann::json;

void BinaryData::ReadFile(const char *data_file) {
  CsvParser *csvparser = CsvParser_new(data_file, ",", 0);
  if (csvparser == nullptr) {
    std::cerr << "Failed to load csv" << std::endl;
    exit(1);
  }
  CsvRow *row;
  data_size_ = 0;
  variable_size_ = 0;
  while ((row = CsvParser_getRow(csvparser))) {
    const char **rowFields = CsvParser_getFields(row);
    std::bitset<MAX_VAR> cur_data;
    cur_data.reset();
    for (int i = 0; i < CsvParser_getNumFields(row); i++) {
      auto cur_item = atoi(rowFields[i]);
      if (cur_item) {
        cur_data.set((size_t) (i + 1));
      }
    }
    if (data_.find(cur_data) != data_.end()) {
      data_[cur_data] += 1;
    } else {
      data_[cur_data] = 1;
    }
    data_size_ += 1;
    if (variable_size_ == 0) {
      variable_size_ = (uint32_t) CsvParser_getNumFields(row);
    } else {
      assert(variable_size_ == (uint32_t) CsvParser_getNumFields(row));
    }
    CsvParser_destroy_row(row);
  }
  CsvParser_destroy(csvparser);
}
void BinaryData::WriteFile(const char *data_file) {
  std::ofstream data_fd;
  data_fd.open(data_file);
  for (const auto &record_pair : data_) {
    auto cur_dat = record_pair.first;
    for (auto j = 0; j < record_pair.second; j++) {
      std::string cur_line;
      for (auto k = 1; k <= variable_size_; k++) {
        cur_line += std::to_string(cur_dat[k]) + ",";
      }
      cur_line.pop_back();
      data_fd << cur_line << std::endl;
    }
  }
  data_fd.close();
}

uintmax_t BinaryData::data_size() const {
  return data_size_;
}

uint32_t BinaryData::variable_size() const {
  return variable_size_;
}

void BinaryData::AddRecord(const std::bitset<MAX_VAR> &dat) {
  if (data_.find(dat) != data_.end()) {
    data_[dat] += 1;
  } else {
    data_[dat] = 1;
  }
  data_size_ += 1;
}

void BinaryData::set_variable_size(uint32_t var_size) {
  variable_size_ = var_size;
}

const std::unordered_map<std::bitset<MAX_VAR>, uintmax_t> &BinaryData::data() const {
  return data_;
}

double BinaryData::CalculateEntropy() const {
  double ent = 0;
  for (const auto &data_record : data_) {
    double lg_pr = std::log(data_record.second) - std::log(data_size_);
    ent += data_record.second * lg_pr;
  }
  return ent / data_size_;
}

BinaryData *BinaryData::ReadSparseDataJsonFile(const char *data_file) {
  std::ifstream data_input_stream(data_file);
  json data_input_json;
  data_input_stream >> data_input_json;
  if (data_input_json.find("variable_size") == data_input_json.end()) {
    std::cerr << "Cannot find \"variable_size\" field in json input " << std::endl;
    return nullptr;
  }
  auto variable_size = data_input_json["variable_size"];
  if (data_input_json.find("data") == data_input_json.end()) {
    std::cerr << "Cannot find \"data\" field in the json input" << std::endl;
    return nullptr;
  }
  const auto &data = data_input_json["data"];
  if (!data.is_array()) {
    std::cerr << "data is not an array in the json input" << std::endl;
  }
  auto new_dataset = new BinaryData();
  new_dataset->set_variable_size((uint32_t) variable_size);
  size_t iteration_index = 0;
  for (auto it = data.begin(); it != data.end(); ++it) {
    const auto &cur_example = *it;
    if (!cur_example.is_array()) {
      std::cerr << "The " << iteration_index << "example from the json input does not form a valid example. Skipped."
                << std::endl;
      continue;
    }
    std::bitset<MAX_VAR> cur_example_bitset;
    bool legal_data = true;
    for (auto positive_variable_it = cur_example.begin(); positive_variable_it != cur_example.end();
         ++positive_variable_it) {
      const auto &cur_variable = *positive_variable_it;
      if (cur_variable.is_number()) {
        if (cur_variable <= variable_size) {
          cur_example_bitset.set(cur_variable);
        } else {
          legal_data = false;
          break;
        }
      } else {
        legal_data = false;
        break;
      }
    }
    if (!legal_data) {
      std::cerr << "The " << iteration_index << "example from the json input does not form a valid example. Skipped."
                << std::endl;
      continue;
    }
    new_dataset->AddRecord(cur_example_bitset);
    iteration_index += 1;
  }
  data_input_stream.close();
  return new_dataset;
}


