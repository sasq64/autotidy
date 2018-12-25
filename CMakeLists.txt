cmake_minimum_required(VERSION 3.5)
project(autotidy LANGUAGES CXX)

# Global project settings
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GCC/Clang common warnings
set(GCC_CLANG_WARNINGS
    -Wall
    -Wextra
    -Wnon-virtual-dtor
    -pedantic
    -Wpedantic
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

# EXTERNAL

# External dependencies
add_subdirectory(external/fmt)
add_subdirectory(external/abseil-cpp)
add_subdirectory(external/CLI11)
add_subdirectory(external/yaml-cpp)

add_executable(tidytest src/testmain.cpp src/patched_file.test.cpp)
target_compile_options(tidytest PRIVATE ${WARNINGS})
target_link_libraries(tidytest PRIVATE absl::strings absl::algorithm)

add_executable(autotidy src/main.cpp src/autotidy.cpp)
target_compile_options(autotidy PRIVATE ${WARNINGS})
target_include_directories(autotidy PRIVATE external)
target_link_libraries(autotidy PRIVATE fmt absl::strings CLI11 yaml-cpp)
