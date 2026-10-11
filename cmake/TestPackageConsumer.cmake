# Installs the package built in BUILD_DIR into a folder of its own, then configures, builds and runs
# tests/package_consumer against it, as a project of someone else's would: find_package(Olagarro) and its components.
# Run by ctest as Olagarro.PackageConsumer, with BUILD_DIR, SOURCE_DIR, WORK_DIR, GENERATOR and CXX_COMPILER given.
file(REMOVE_RECURSE "${WORK_DIR}")
foreach(step
        "${CMAKE_COMMAND};--install;${BUILD_DIR};--prefix;${WORK_DIR}/install"
        "${CMAKE_COMMAND};-S;${SOURCE_DIR}/tests/package_consumer;-B;${WORK_DIR}/build;-G;${GENERATOR};-DCMAKE_CXX_COMPILER=${CXX_COMPILER};-DCMAKE_PREFIX_PATH=${WORK_DIR}/install"
        "${CMAKE_COMMAND};--build;${WORK_DIR}/build"
        "${WORK_DIR}/build/olagarro_package_consumer")
    execute_process(COMMAND ${step} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE output)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${step}\n${output}")
    endif()
endforeach()
file(REMOVE_RECURSE "${WORK_DIR}")
