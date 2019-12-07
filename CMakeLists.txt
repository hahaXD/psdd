cmake_minimum_required(VERSION 3.9)
project(psdd)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 99)
include_directories(include)

if(APPLE)
  link_directories(lib/darwin)
endif(APPLE)

if(UNIX AND NOT APPLE)
  link_directories(lib/linux)
endif(UNIX AND NOT APPLE)  

set(gtest_main "libgtest_main.a")
set(gtest "libgtest.a")
set(sdd "libsdd.a")
set(gmock "libgmock.a")
set(gmock_main "libgmock_main.a")

file(GLOB PSDD_TEST_SRC_FILES "test/*.cpp")
file(GLOB LIB_PSDD_SRC_FILES "src/*.h" "src/*.cpp" "src/*.c")
#add_executable(psdd_test ${PSDD_TEST_SRC_FILES})
add_library(psdd ${LIB_PSDD_SRC_FILES} )
add_executable(psdd_inference psdd_inference_main.cpp)
#target_link_libraries(psdd_test psdd ${gtest} ${gtest_main} ${gmock} ${gmock_main} ${sdd} gmp pthread)
target_link_libraries(psdd_inference psdd sdd gmp)
