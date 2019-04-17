cmake_minimum_required(VERSION 3.5)
project(autotidy LANGUAGES CXX)

# Global project settings
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable ccache if available
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

# GCC/Clang common warnings
set(GCC_CLANG_WARNINGS
    -Wall
    -Wextra
    -Wnon-virtual-dtor
    -Wcast-align)

# GCC specific warnings
set(GCC_WARNINGS
    -Wduplicated-branches
    -Wduplicated-cond
    -Wlogical-op
    -Wnull-dereference
    -Wno-missing-field-initializers)

# MSVC warnings
set(MSVC_WARNINGS /W4 /w14640 /permissive)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(WARNINGS ${WARNINGS} ${GCC_WARNINGS} ${GCC_CLANG_WARNINGS})
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(WARNINGS ${WARNINGS} ${CLANG_WARNINGS} ${GCC_CLANG_WARNINGS})
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  set(WARNINGS ${WARNINGS} ${MSVC_WARNINGS})
endif()

add_library(Warnings INTERFACE)
target_compile_options(Warnings INTERFACE ${WARNINGS})

# External dependencies
add_subdirectory(external/fmt)
add_subdirectory(external/abseil-cpp)
add_subdirectory(external/CLI11)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Enable testing")
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "Enable parse tools")
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "Enable contrib stuff in library")
set(YAML_CPP_INSTALL OFF CACHE BOOL "Enable generation of install target")
add_subdirectory(external/yaml-cpp)

add_executable(makedoc src/makedoc.cpp)
target_link_libraries(makedoc PRIVATE Warnings fmt absl::strings)

add_executable(tidytest src/testmain.cpp src/patched_file.test.cpp)
target_link_libraries(tidytest PRIVATE Warnings absl::strings absl::algorithm)

add_executable(autotidy src/main.cpp src/autotidy.cpp src/manpages.cpp)
target_link_libraries(autotidy PRIVATE Warnings fmt absl::strings CLI11 yaml-cpp)
