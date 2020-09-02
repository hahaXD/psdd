#ifndef PSDD_UAI_NETWORK_H
#define PSDD_UAI_NETWORK_H

#include <unordered_set>
#include <vector>

#include "psdd/psdd_parameter.h"

#define MARKOV_NETWORK_TYPE 1
#define BAYESIAN_NETWORK_TYPE 2

class UaiNetwork {
 public:
  UaiNetwork();
  UaiNetwork(size_t var_size, size_t factor_size, int network_type,
             std::vector<std::vector<size_t>> clusters,
             std::vector<std::vector<PsddParameter>> params);
  void read_file(
      const char* uai_file);  // uai file, variables appearing in the same
                              // factor, the last varaible will be the LSB,
                              // where mod/2 corresponding to its value.
  size_t var_size();
  size_t factor_size();
  int network_type() const;
  const std::vector<std::vector<size_t>>& factor_scopes() const;
  const std::vector<std::vector<PsddParameter>>& params() const;

 private:
  size_t m_var_size;
  size_t m_factor_size;
  int m_network_type;  // 1 is Mark, 2 is BN
  std::vector<std::vector<size_t>> m_clusters;
  std::vector<std::vector<PsddParameter>> m_params;
};

#endif  // PSDD_UAI_NETWORK_HPP
