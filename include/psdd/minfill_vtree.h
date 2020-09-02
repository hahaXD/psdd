#ifndef MINFILL_VTREE_H
#define MINFILL_VTREE_H

#include "psdd/uai_network.h"

extern "C" {
#include <sdd/sddapi.h>
}

class MinFillVtree {
 public:
  MinFillVtree(UaiNetwork* network);
  Vtree* ConstructVtree();

 private:
  UaiNetwork* network_;
};
#endif
