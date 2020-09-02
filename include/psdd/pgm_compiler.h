#ifndef PGM_COMPILER_H
#define PGM_COMPILER_H
#include <string>

#include "psdd/psdd_manager.h"
#include "psdd/uai_network.h"

#define VTREE_METHOD_MINFILL 4
#define VTREE_METHOD_HYPER_FIXED_BF 1
#define VTREE_METHOD_JOINTREE 2

class PgmCompiler {
 public:
  PgmCompiler(std::string working_dir);
  void init_psdd_manager(char mode);
  void init_psdd_manager_from_vtree(const char *vtree_fname);
  void read_uai_file(const char *uai_file);
  PsddManager *psdd_manager() const;
  std::pair<PsddNode *, PsddParameter> compile_factor(size_t factor_index);
  std::pair<PsddNode *, PsddParameter> compile_network(size_t gc_freq);
  std::pair<PsddNode *, PsddParameter> compile_network_with_vtree(
      size_t gc_freq);
  std::pair<PsddNode *, PsddParameter> compile_network_dc(size_t gc_freq);

 private:
  UaiNetwork *m_network;
  PsddManager *m_pm;
  std::string working_dir_;
};

#endif
