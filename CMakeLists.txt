# CMakeList.txt: ProfileEncoder 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.30)

# 如果支持，请为 MSVC 编译器启用热重载。
if(POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Enable import std support
if(CMAKE_VERSION VERSION_LESS "3.31.8")
  # CMake 3.30.0 - 3.31.7
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b3d-a41c-cf096e0b2508")
elseif(CMAKE_VERSION VERSION_LESS "4.0.0")
  # CMake 3.31.8 - 3.31.12
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
elseif(CMAKE_VERSION VERSION_LESS "4.0.3")
  # CMake 4.0.0 - 4.0.2
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "a9e1cf81-9932-4810-974b-6eccaf14e457")
elseif(CMAKE_VERSION VERSION_LESS "4.3.0")
  # CMake 4.0.3 - 4.2.7
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
elseif(CMAKE_VERSION VERSION_LESS "4.4.0")
  # CMake 4.3.x
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
elseif(CMAKE_VERSION VERSION_LESS "4.4.3")
  # CMake 4.4.0 - 4.4.2
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "f35a9ac6-8463-4d38-8eec-5d6008153e7d")
else()
  message(FATAL_ERROR
          "Unsupported CMake ${CMAKE_VERSION}: "
          "check https://github.com/Kitware/CMake/blob/master/Help/dev/experimental.rst for the current import std UUID")
endif()

# Project definition
project("ProfileEncoder" LANGUAGES CXX)

# Make sure C++ 23 is available
if(NOT 23 IN_LIST CMAKE_CXX_COMPILER_IMPORT_STD)
  message(FATAL_ERROR
          "The selected toolchain does not support import std in C++23 mode")
endif()

# Packages
# p-ranav/argpase
include(FetchContent)
FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse.git
  GIT_TAG v3.2
)
FetchContent_MakeAvailable(argparse)

add_library(profile_argparse_module STATIC)

target_sources(profile_argparse_module
  PUBLIC
    FILE_SET CXX_MODULES
      BASE_DIRS
        "${argparse_SOURCE_DIR}/module"
      FILES
        "${argparse_SOURCE_DIR}/module/argparse.cppm"
)
target_link_libraries(profile_argparse_module
  PRIVATE
    argparse::argparse
)

target_compile_features(profile_argparse_module PUBLIC cxx_std_23)

# 让 argparse.cppm 自己使用 import std。
target_compile_definitions(profile_argparse_module
  PRIVATE
    ARGPARSE_MODULE_USE_STD_MODULE
)

set_target_properties(profile_argparse_module PROPERTIES
  CXX_MODULE_STD ON
  CXX_EXTENSIONS OFF
)

# Boost::process
find_package(Boost 1.86 REQUIRED COMPONENTS process)

# 将源代码添加到此项目的可执行文件。
add_executable(ProfileEncoder "src/ProfileEncoder.cpp")

target_link_libraries(ProfileEncoder
  PRIVATE
    profile_argparse_module
    Boost::process
)

target_sources(ProfileEncoder
  PRIVATE
    FILE_SET CXX_MODULES
    FILES
      "src/logger.ixx"
      "src/helpers.ixx"
      "src/process_runner.ixx"
)

set_target_properties(ProfileEncoder 
  PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  CXX_SCAN_FOR_MODULES ON
  CXX_MODULE_STD ON
)

target_compile_features(ProfileEncoder PRIVATE cxx_std_23)

