cmake_minimum_required(VERSION 3.14)
project(cttasks_tests)

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

include_directories(../../cttasks/include)

add_subdirectory(../../cttasks ../../cttasks/bin)

enable_testing()

add_executable(cttasks_tests
  containertasks_test.cpp
  actionexcutor_test.cpp
)

target_link_libraries(cttasks_tests
  PRIVATE GTest::gtest_main
  PRIVATE GTest::gmock_main
  PRIVATE cttasks
)

include(GoogleTest)
gtest_discover_tests(cttasks_tests)
