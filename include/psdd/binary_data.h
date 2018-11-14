//
// Created by Yujia Shen on 10/29/17.
//

#ifndef STRUCTURED_BAYESIAN_NETWORK_BINARY_DATA_H
#define STRUCTURED_BAYESIAN_NETWORK_BINARY_DATA_H

#include <bitset>
#include <cstddef>
#include <unordered_map>
#include <vector>

#define MAX_VAR 65536

//#define MAX_VAR 16384

class BinaryData {
public:
  static BinaryData *ReadSparseDataJsonFile(const char *data_file);
  void ReadFile(const char *data_file);
  void WriteFile(const char *data_file);
  uintmax_t data_size() const;
  uint32_t variable_size() const;
  void AddRecord(const std::bitset<MAX_VAR> &dat);
  void set_variable_size(uint32_t var_size);
  const std::unordered_map<std::bitset<MAX_VAR>, uintmax_t> &data() const;
  double CalculateEntropy() const;
  void SetMask(std::bitset<MAX_VAR> variable_mask) {
    variable_mask_ = variable_mask;
  }
  const std::bitset<MAX_VAR> variable_mask() { return variable_mask_; }

private:
  std::unordered_map<std::bitset<MAX_VAR>, uintmax_t> data_;
  uint32_t variable_size_;
  uintmax_t data_size_;
  std::bitset<MAX_VAR> variable_mask_;
};

#endif // STRUCTURED_BAYESIAN_NETWORK_BINARY_DATA_H
