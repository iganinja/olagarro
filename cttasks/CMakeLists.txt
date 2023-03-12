cmake_minimum_required(VERSION 3.14)

project(cttasks LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(cttasks STATIC
  src/wait.cpp

  include/cttasks/taskscommon.h
  include/cttasks/mputils.h
  include/cttasks/taskexecutor.h
  include/cttasks/serial.h
  include/cttasks/parallel.h
  include/cttasks/earlyexitparallel.h
  include/cttasks/execute.h
  include/cttasks/wait.h
  include/cttasks/repeat.h
  include/cttasks/select.h
)

include_directories(include)

target_compile_definitions(cttasks PUBLIC CTTASKS_LIBRARY)

install(TARGETS cttasks DESTINATION lib)
