cmake_minimum_required(VERSION 3.14)
project(tasks_tests)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
  googlemock
  GIT_REPOSITORY https://github.com/google/googlemock.git
  GIT_TAG release-1.7.0
)
#set(BUILD_GMOCK ON)
# For Windows: Prevent overriding the parent project's compiler/linker settings
#set(gmock_force_shared_crt ON CACHE BOOL "" FORCE)
#FetchContent_MakeAvailable(googlemock)

include_directories(../../tasks/include)

add_subdirectory(../../tasks ../../tasks/bin)

enable_testing()

add_executable(tasks_tests
  taskexecutor_test.cpp
  containeractions_test.cpp
  wait_test.cpp

  task_mock.h
)

target_link_libraries(tasks_tests
  PRIVATE GTest::gtest_main
  PRIVATE GTest::gmock_main
  PRIVATE tasks
)

include(GoogleTest)
gtest_discover_tests(tasks_tests)
