//
// Created by Jason Shen on 4/22/18.
//

#include <iostream>
#include <psdd/cnf.h>
#include <psdd/optionparser.h>
extern "C" {
#include <sdd/sddapi.h>
}
struct Arg : public option::Arg {
  static void printError(const char *msg1, const option::Option &opt,
                         const char *msg2) {
    fprintf(stderr, "%s", msg1);
    fwrite(opt.name, (size_t)opt.namelen, 1, stderr);
    fprintf(stderr, "%s", msg2);
  }

  static option::ArgStatus Required(const option::Option &option, bool msg) {
    if (option.arg != 0)
      return option::ARG_OK;

    if (msg)
      printError("Option '", option, "' requires an argument\n");
    return option::ARG_ILLEGAL;
  }

  static option::ArgStatus Numeric(const option::Option &option, bool msg) {
    char *endptr = 0;
    if (option.arg != 0 && strtol(option.arg, &endptr, 10)) {
    };
    if (endptr != option.arg && *endptr == 0)
      return option::ARG_OK;

    if (msg)
      printError("Option '", option, "' requires a numeric argument\n");
    return option::ARG_ILLEGAL;
  }
};
enum optionIndex {
  UNKNOWN,
  HELP,
  MPE_QUERY,
  MAR_QUERY,
  CNF_EVID,
  SPARSE_DATA_FILENAME
};

const option::Descriptor usage[] = {
    {UNKNOWN, 0, "", "", option::Arg::None,
     "USAGE: example [options]\n\n \tOptions:"},
    {HELP, 0, "h", "help", option::Arg::None,
     "--help  \tPrint usage and exit."},
    {MPE_QUERY, 0, "", "mpe_query", option::Arg::None, ""},
    {MAR_QUERY, 0, "", "mar_query", option::Arg::None, ""},
    {CNF_EVID, 0, "", "cnf_evid", Arg::Required,
     "--cnf_evid  evid file, represented using CNF."},
    {SPARSE_DATA_FILENAME, 0, "", "sparse_data_filename", Arg::Required,
     "--sparse_data_filename sparse data filename to check consistency."},
    {UNKNOWN, 0, "", "", option::Arg::None,
     "\nExamples:\n./psdd_inference  psdd_filename vtree_filename \n"},
    {0, 0, 0, 0, 0, 0}};

int main(int argc, const char *argv[]) {
  argc -= (argc > 0);
  argv += (argc > 0); // skip program name argv[0] if present
  option::Stats stats(usage, argc, argv);
  std::vector<option::Option> options(stats.options_max);
  std::vector<option::Option> buffer(stats.buffer_max);
  option::Parser parse(usage, argc, argv, &options[0], &buffer[0]);
  if (parse.error())
    return 1;
  if (options[HELP] || argc == 0) {
    option::printUsage(std::cout, usage);
    return 0;
  }
  const char *psdd_filename = parse.nonOption(0);
  const char *vtree_filename = parse.nonOption(1);
  CNF *cnf = nullptr;
  if (options[CNF_EVID]) {
    const char *cnf_filename = options[CNF_EVID].arg;
    cnf = new CNF(cnf_filename);
  }
  Vtree *psdd_vtree = sdd_vtree_read(vtree_filename);
  PsddManager *psdd_manager = PsddManager::GetPsddManagerFromVtree(psdd_vtree);
  sdd_vtree_free(psdd_vtree);
  PsddNode *result_node = psdd_manager->ReadPsddFile(psdd_filename, 0);
  if (options[SPARSE_DATA_FILENAME]) {
    std::vector<PsddNode *> serialized_psdd_nodes =
        psdd_node_util::SerializePsddNodes(result_node);
    const char *sparse_data_filename = options[SPARSE_DATA_FILENAME].arg;
    BinaryData *sparse_data =
        BinaryData::ReadSparseDataJsonFile(sparse_data_filename);
    const auto &data_record = sparse_data->data();
    uintmax_t satisfied_records = 0;
    uintmax_t inconsistent_records = 0;
    std::bitset<MAX_VAR> variable_mask = sparse_data->variable_mask();
    for (const auto &record_pair : data_record) {
      if (psdd_node_util::IsConsistent(serialized_psdd_nodes, variable_mask, record_pair.first)){
        satisfied_records += record_pair.second;
      }else{
        inconsistent_records += record_pair.second;
      }
    }
    std::cout << "Consistent Data" << satisfied_records << " Inconsistent Data " << inconsistent_records << std::endl;
    delete sparse_data;
  }
  if (cnf != nullptr) {
    PsddNode *evid = cnf->Compile(psdd_manager, 0);
    auto new_node_result = psdd_manager->Multiply(evid, result_node, 0);
    result_node = new_node_result.first;
  }
  if (result_node == nullptr) {
    std::cout << "UNSATISFIED" << std::endl;
    exit(0);
  }
  std::vector<SddLiteral> variables =
      vtree_util::VariablesUnderVtree(psdd_manager->vtree());
  auto serialized_psdd = psdd_node_util::SerializePsddNodes(result_node);
  if (options[MPE_QUERY]) {
    auto mpe_result = psdd_node_util::GetMPESolution(serialized_psdd);
    std::cout << "MPE result=";
    for (SddLiteral variable_index : variables) {
      std::cout << variable_index << ":" << mpe_result.first[variable_index]
                << ",";
    }
    std::cout << std::endl;
  }
  if (options[MAR_QUERY]) {
    auto mar_result = psdd_node_util::GetMarginals(serialized_psdd);
    std::cout << "MAR result=";
    for (auto result_pair : mar_result) {
      std::cout << result_pair.first << ":"
                << result_pair.second.first.parameter() << "|"
                << result_pair.second.second.parameter() << ",";
    }
    std::cout << std::endl;
  }
  delete (psdd_manager);
  if (cnf != nullptr) {
    delete (cnf);
  }
}
