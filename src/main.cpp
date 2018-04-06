//
// Created by Yujia Shen on 3/20/18.
//
extern "C"{
#include <sddapi.h>
}
#include <iostream>
int main(){
  Vtree* v = sdd_vtree_new(10, "balanced");
  SddManager* manager = sdd_manager_new(v);
  SddNode* cur_node = sdd_manager_true(manager);
  std::cout << "Model count " << sdd_global_model_count(cur_node, manager) << std::endl;
  sdd_manager_free(manager);
}