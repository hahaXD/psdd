#ifndef JOINTREE_VTREE_H
#define JOINTREE_VTREE_H
#include "psdd/uai_network.h"

extern "C" {
#include <sdd/sddapi.h>
}

class JointreeVtree {
 public:
  JointreeVtree(UaiNetwork* network) : network_{network} {}
  Vtree* ConstructVtree();

 private:
  UaiNetwork* network_;
};
#endif
