//
// Created by Jason Shen on 4/22/18.
//

#include <stdio.h>

#include <algorithm>
#include <cstring>
#include <iostream>

#include "psdd/pgm_compiler.h"

int main(int argc, const char* argv[]) {
  if (argc < 3) {
    std::cout << "Usage <uai_fname> <vtree_method>" << std::endl;
    std::cout << "vtree_method can be\n";
    std::cout << "  1 (hyper tree partition that only works on linux) \n";
    std::cout
        << "  4 (vtree from minfill order that works on both mac and linux) \n";
    std::cout
        << "  others (the argument is interpreted as a vtree filename) \n";
    std::cout << "Psdd and Vtree files are stored as the name of "
                 "<uai_fname>.psdd and <uai_fname>.vtree"
              << std::endl;
    exit(1);
  }
  const char* uai_fname = argv[1];
  const char* vtree_method = argv[2];
  bool gen_vtree = false;
  char vtree_method_idx = 0;
  if (strcmp(vtree_method, "1") == 0) {
    vtree_method_idx = VTREE_METHOD_HYPER_FIXED_BF;
    gen_vtree = true;
  } else if (strcmp(vtree_method, "4") == 0) {
    vtree_method_idx = VTREE_METHOD_MINFILL;
    gen_vtree = true;
  } else {
    vtree_method_idx = -1;
  }

  PgmCompiler pc;
  pc.read_uai_file(uai_fname);
  if (gen_vtree) {
    pc.init_psdd_manager(vtree_method_idx);
  } else {
    pc.init_psdd_manager_from_vtree(vtree_method);
  }
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
  std::cout << "Log Partition" << result.second.parameter() << std::endl;
}
