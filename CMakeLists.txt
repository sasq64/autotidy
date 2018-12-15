cmake_minimum_required(VERSION 3.5)
project(ecp)

# Global project settings
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ANDROID_STL_TYPE c++_static)

if(IOS_PLATFORM)
  set(IOS 1)
  message("IOS")
endif()

if(ANDROID OR WIN32 OR IOS)
  set(BUILD_TOOLS 0)
else()
  set(BUILD_TOOLS 1)
endif()

# GCC/Clang common warnings
set(GCC_CLANG_WARNINGS -Wall -Wextra -Wnon-virtual-dtor -pedantic -Wpedantic -Wcast-align)

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

message("${WARNINGS}")
message("${CMAKE_CXX_COMPILER_ID}")

# EXTERNAL

# External dependencies
add_subdirectory(external/coreutils)
add_subdirectory(external/lua)
add_subdirectory(external/sol2)
add_subdirectory(external/fmt)
add_subdirectory(external/json)

# SRC

# Statemachine
set(STATEMACHINE_DIR src/statemachine)
add_library(statemachine
            STATIC
            ${STATEMACHINE_DIR}/state_machine.cpp
            ${STATEMACHINE_DIR}/component.cpp
            ${STATEMACHINE_DIR}/component_impl.cpp)
target_include_directories(statemachine INTERFACE ${STATEMACHINE_DIR})
target_compile_definitions(statemachine PRIVATE SOL_USING_CXX_LUA)
target_compile_options(statemachine PRIVATE ${WARNINGS})
target_link_libraries(statemachine PRIVATE lua coreutils)

# Ecpappliance
set(ECPAPPLIANCE_DIR src/ecpappliance)
add_library(ecpappliance
            STATIC
            ${ECPAPPLIANCE_DIR}/ecpappliance.cpp
            ${ECPAPPLIANCE_DIR}/ecpcomponent.cpp
            ${ECPAPPLIANCE_DIR}/send_command.cpp
            ${ECPAPPLIANCE_DIR}/builder.cpp
            ${ECPAPPLIANCE_DIR}/transform.cpp)
target_link_libraries(ecpappliance PUBLIC statemachine coreutils json)
target_include_directories(ecpappliance INTERFACE ${ECPAPPLIANCE_DIR})
target_compile_options(ecpappliance PRIVATE ${WARNINGS})

if(BUILD_TOOLS)


  add_executable(tidy src/tidy.cpp)
  target_compile_options(tidy PRIVATE ${WARNINGS})
  target_link_libraries(tidy
                        PRIVATE
                        fmt
                        coreutils)

  # Tool
  add_executable(tool ${ECPAPPLIANCE_DIR}/tool.cpp)
  target_compile_options(tool PRIVATE ${WARNINGS})
  target_link_libraries(tool
                        PRIVATE
                        readline
                        ecpappliance
                        coreutils)

  # Benchmark
  add_subdirectory(external/benchmark)
  add_executable(speedtest ${ECPAPPLIANCE_DIR}/speedtest.cpp)
  target_link_libraries(
    speedtest PRIVATE readline ecpappliance coreutils benchmark)

  # Unit tests
  add_executable(ecptest
                 src/test/testmain.cpp
                 ${ECPAPPLIANCE_DIR}/builder.test.cpp
                 ${STATEMACHINE_DIR}/state_machine.test.cpp)
  target_compile_options(ecptest PRIVATE ${WARNINGS})
  target_link_libraries(ecptest PRIVATE ecpappliance statemachine)
  target_include_directories(ecptest PRIVATE external/catch2)
endif()

# JNI LIBRARY

if(NOT IOS)

  if(NOT ANDROID)
    find_package(JNI REQUIRED)
    set(JNI_INCLUDE_PATHS ${JAVA_INCLUDE_PATH})
    if(WIN32)
      set(JNI_INCLUDE_PATHS ${JNI_INCLUDE_PATHS} ${JAVA_INCLUDE_PATH}/win32)
    else()
      set(JNI_INCLUDE_PATHS ${JNI_INCLUDE_PATHS} ${JAVA_INCLUDE_PATH}/linux)
    endif()
  endif()

  set(SM_SOURCES
      generated/jni/jni_Component.cpp
      generated/jni/jni_Step.cpp
      generated/jni/jni_StateMachine.cpp
      generated/jni/jni_Holder.cpp
      src/djinni/djinni_statemachine.cpp
      extras/support-lib/jni/djinni_support.cpp
      extras/support-lib/jni/djinni_main.cpp)
  add_library(sm SHARED ${SM_SOURCES})
  target_link_libraries(sm PRIVATE ecpappliance $<$<BOOL:${ANDROID}>:log>)
  target_include_directories(
    sm PRIVATE generated/cpp extras/support-lib/jni ${JNI_INCLUDE_PATHS})

endif()
