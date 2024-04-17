# Require CMake 3.15+ (matching scikit-build-core) Use new versions of all
# policies up to CMake 3.27
cmake_minimum_required(VERSION 3.15...3.26)

# Scikit-build-core sets these values for you, or you can just hard-code the
# name and version.
project(
  ${SKBUILD_PROJECT_NAME}
  VERSION ${SKBUILD_PROJECT_VERSION}
  LANGUAGES CXX)

# Find the module development requirements (requires FindPython from 3.17 or
# scikit-build-core's built-in backport)
find_package(
  Python 3.8 REQUIRED
  COMPONENTS Interpreter Development.Module
  OPTIONAL_COMPONENTS Development.SABIModule)
find_package(nanobind CONFIG REQUIRED)

nanobind_add_module(
  _ext
  STABLE_ABI
  NB_STATIC
  src/cpp/module.cpp
  src/cpp/Pipe.cpp
  src/cpp/PipeConnection.cpp
  src/cpp/util.cpp)

set_property(TARGET _ext PROPERTY CXX_STANDARD 20)
target_compile_definitions(_ext PRIVATE VERSION_INFO=${PROJECT_VERSION})

# Install directive for scikit-build-core
install(TARGETS _ext LIBRARY DESTINATION win_pipes)