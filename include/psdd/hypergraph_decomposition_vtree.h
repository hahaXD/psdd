#ifndef HYPERGRAPH_DECOMPOSITION_VTREE_H
#define HYPERGRAPH_DECOMPOSITION_VTREE_H
#include "psdd/uai_network.h"

extern "C" {
#include <sdd/sddapi.h>
}

class HypergraphDecompositionVtree {
 public:
  HypergraphDecompositionVtree(UaiNetwork* network, const char* working_dir)
      : network_(network), working_dir_(working_dir) {}
  Vtree* ConstructVtree();

 private:
  UaiNetwork* network_;
  const char* working_dir_;
};
#endif
