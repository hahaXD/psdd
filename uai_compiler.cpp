//
// Created by Jason Shen on 4/22/18.
//

#include <stdio.h>

#include <algorithm>
#include <iostream>

#include "psdd/pgm_compiler.h"

int main(int argc, const char* argv[]) {
  const char* uai_fname = argv[1];
  PgmCompiler pc;
  pc.read_uai_file(uai_fname);
  pc.init_psdd_manager(VTREE_METHOD_HYPER_FIXED_BF);
  auto result = pc.compile_network_dc(100);

  // output filename
  char psdd_fname[1000];
  char vtree_fname[1000];
  sprintf(psdd_fname, "%s.psdd", uai_fname);
  sprintf(vtree_fname, "%s.vtree", uai_fname);

  psdd_node_util::WritePsddToFile(result.first, psdd_fname);
  sdd_vtree_save(vtree_fname, pc.psdd_manager()->vtree());
  std::cout << "Final size "
            << psdd_node_util::SerializePsddNodes(result.first).size()
            << std::endl;
  std::cout << result.second.parameter() << std::endl;
}
