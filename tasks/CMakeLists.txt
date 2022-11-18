cmake_minimum_required(VERSION 3.14)

project(tasks LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(tasks STATIC
  src/task.cpp
  src/taskexecutor.cpp
  src/serial.cpp
  src/parallel.cpp
  src/earlyexitparallel.cpp
  src/wait.cpp
  src/select.cpp
  src/repeat.cpp

  include/tasks/task.h
  include/tasks/taskexecutor.h
  include/tasks/serial.h
  include/tasks/parallel.h
  include/tasks/earlyexitparallel.h
  include/tasks/taskscore.h
  include/tasks/wait.h
  include/tasks/select.h
  include/tasks/execute.h
  include/tasks/repeat.h
)

include_directories(include)

target_compile_definitions(tasks PUBLIC TASKS_LIBRARY)

install(TARGETS tasks DESTINATION lib)
