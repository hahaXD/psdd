cmake_minimum_required(VERSION 3.9)
project(psdd)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 99)
include_directories(src include)

include(ExternalProject)
ExternalProject_Add(sdd_github
  GIT_REPOSITORY   https://github.com/hahaXD/sdd.git
  GIT_TAG          master
  STEP_TARGETS build
  EXCLUDE_FROM_ALL TRUE
  )
add_library(sdd UNKNOWN IMPORTED)
ExternalProject_Get_Property(sdd_github binary_dir)
set_target_properties(sdd PROPERTIES IMPORTED_LOCATION ${binary_dir}/libsdd.a)
add_dependencies(sdd sdd_github-build)

ExternalProject_Add(googletest_github
  GIT_REPOSITORY   https://github.com/google/googletest.git
  GIT_TAG          master
  STEP_TARGETS build
  EXCLUDE_FROM_ALL TRUE
  )

ExternalProject_Get_Property(googletest_github binary_dir)
add_library(gtest UNKNOWN IMPORTED)
add_library(gtest_main UNKNOWN IMPORTED)
set_target_properties(gtest PROPERTIES
  IMPORTED_LOCATION ${binary_dir}/googlemock/gtest/libgtest.a
  )
set_target_properties(gtest_main PROPERTIES
  IMPORTED_LOCATION ${binary_dir}/googlemock/gtest/libgtest_main.a
  )
add_dependencies(gtest googletest_github-build)
add_dependencies(gtest_main googletest_github-build)

file(GLOB PSDD_TEST_SRC_FILES "test/*.cpp")
file(GLOB LIB_PSDD_SRC_FILES "src/*.h" "src/*.cpp" "src/*.c")
add_executable(psdd_test ${PSDD_TEST_SRC_FILES})
add_library(psdd ${LIB_PSDD_SRC_FILES} )
add_executable(psdd_inference psdd_inference_main.cpp)
target_link_libraries(psdd_test psdd gtest gtest_main sdd gmp pthread)
target_link_libraries(psdd sdd gmp)
target_link_libraries(psdd_inference psdd sdd gmp)
