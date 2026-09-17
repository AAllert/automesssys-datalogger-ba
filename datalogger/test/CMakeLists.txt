# This is the project CMakeLists.txt file for the test subproject
cmake_minimum_required(VERSION 3.16)

# Include the components directory of the main application:
#
set(EXTRA_COMPONENT_DIRS "../components")

# Set the components to include the tests for.
# This can be overriden from CMake cache:
# - when invoking CMake directly: cmake -D TEST_COMPONENTS="xxxxx" ..
# - when using idf.py: idf.py -T xxxxx build
#
set(TEST_COMPONENTS "can" "can_backend" "datalogger_service" "log_manager" "storage" "uds" "uds_config" "uds_decoder" "utils" CACHE STRING "List of components to test")

# Enables --coverage instrumentation on TEST_COMPONENTS and the gcovr-report
# build target. Set via: idf.py -D TEST_COVERAGE=1 build
set(TEST_COVERAGE OFF CACHE BOOL "Instrument TEST_COMPONENTS for gcov code coverage")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(datalogger_esp)

if(TEST_COVERAGE)
    idf_create_coverage_report(${CMAKE_CURRENT_BINARY_DIR}/coverage_report)
    idf_clean_coverage_report(${CMAKE_CURRENT_BINARY_DIR}/coverage_report)

    foreach(component ${TEST_COMPONENTS})
        idf_component_get_property(component_lib ${component} COMPONENT_LIB)
        target_compile_options(${component_lib} PRIVATE --coverage)
        target_link_options(${component_lib} INTERFACE --coverage)
    endforeach()
endif()