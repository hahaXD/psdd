//
// Created by Yujia Shen on 4/8/18.
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <unordered_map>
#include <psdd/psdd_manager.h>
extern "C" {
#include <sdd/sddapi.h>
}

TEST(VTREE_TEST, VTREE_PROJECTION_TEST) {
  Vtree *new_vtree = sdd_vtree_new(10, "right");
  std::vector<SddLiteral> total;
  for (auto i = 0; i < 10; i++) {
    total.push_back(i + 1);
  }
  Vtree *projected_vtree = vtree_util::ProjectVtree(new_vtree, total);
  EXPECT_NE(projected_vtree, new_vtree);
  std::vector<SddLiteral> variables_under_new_vtree = vtree_util::VariablesUnderVtree(projected_vtree);
  EXPECT_THAT(variables_under_new_vtree, testing::WhenSorted(testing::ElementsAre(1, 2, 3, 4, 5, 6, 7, 8, 9, 10)));
  std::vector<SddLiteral> variables_to_check;
  for (auto i = 0; i < 10; i += 2) {
    variables_to_check.push_back(i + 1);
  }
  Vtree *projected_vtree_2 = vtree_util::ProjectVtree(new_vtree, variables_to_check);
  variables_under_new_vtree = vtree_util::VariablesUnderVtree(projected_vtree_2);
  std::sort(variables_to_check.begin(), variables_to_check.end());
  EXPECT_THAT(variables_under_new_vtree, testing::WhenSorted(testing::ElementsAreArray(variables_to_check)));
  sdd_vtree_free(projected_vtree_2);
  sdd_vtree_free(new_vtree);
  sdd_vtree_free(projected_vtree);
}

TEST(VTREE_TEST, VARIABLES_UNDER_VTREE_TEST) {
  Vtree *new_vtree = sdd_vtree_new(10, "balanced");
  std::vector<SddLiteral> variables = vtree_util::VariablesUnderVtree(new_vtree);
  EXPECT_THAT(variables, testing::WhenSorted(testing::ElementsAre(1, 2, 3, 4, 5, 6, 7, 8, 9, 10)));
  sdd_vtree_free(new_vtree);
}

TEST(VTREE_TEST, SUB_VTREE_FROM_VAR_TEST) {
  Vtree *new_vtree = sdd_vtree_new(10, "right");
  std::vector<SddLiteral> variables = {1};
  Vtree *r1 = vtree_util::SubVtreeByVariables(new_vtree, {1});
  EXPECT_EQ(r1, sdd_vtree_left(new_vtree));
  Vtree *r2 = vtree_util::SubVtreeByVariables(new_vtree, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  EXPECT_EQ(r2, new_vtree);
  Vtree *r3 = vtree_util::SubVtreeByVariables(new_vtree, {2, 3, 4, 5, 6, 7, 8, 9, 10});
  EXPECT_EQ(r3, sdd_vtree_right(new_vtree));
  Vtree *r4 = vtree_util::SubVtreeByVariables(new_vtree, {1, 2, 3, 4, 11});
  EXPECT_EQ(r4, nullptr);
  Vtree *r5 = vtree_util::SubVtreeByVariables(new_vtree, {2, 3, 9, 10});
  EXPECT_EQ(r5, nullptr);
}
