cmake_minimum_required(VERSION 3.16)

PROJECT(dolfinx_pybind11)

# Set C++ standard before finding pybind11
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Development component which includes both Development.Module and
# Development.Embed is not required for building a Python module.  Correct
# COMPONENT specification Development.Module added only in CMake 3.18 and
# above.
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.18.0")
  find_package(Python3 COMPONENTS Interpreter Development.Module REQUIRED)
else()
  find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
endif()
find_package(pybind11 2.7.0 REQUIRED CONFIG HINTS ${PYBIND11_DIR} ${PYBIND11_ROOT}
  $ENV{PYBIND11_DIR} $ENV{PYBIND11_ROOT})

execute_process(
  COMMAND ${Python3_EXECUTABLE} -c "import basix, os, sys; sys.stdout.write(os.path.dirname(basix.__file__))"
  OUTPUT_VARIABLE BASIX_PY_DIR
  RESULT_VARIABLE BASIX_PY_COMMAND_RESULT
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
if (BASIX_PY_DIR)
  message(STATUS "Adding ${BASIX_PY_DIR} to Basix search hints")
  set(xtl_ROOT "${BASIX_PY_DIR};${xtl_ROOT}")
  set(xtensor_ROOT "${BASIX_PY_DIR};${xtensor_ROOT}")
endif()
find_package(Basix REQUIRED CONFIG HINTS ${BASIX_PY_DIR})

find_package(xtensor REQUIRED)
find_package(DOLFINX REQUIRED CONFIG)

# Check if the Basix C++ library was compiled with xtensor SIMD
get_target_property(BASIX_DEFN Basix::basix INTERFACE_COMPILE_DEFINITIONS)
get_target_property(DOLFINX_DEFN dolfinx INTERFACE_COMPILE_DEFINITIONS)
if("XTENSOR_USE_XSIMD" IN_LIST BASIX_DEFN OR "XTENSOR_USE_XSIMD" IN_LIST DOLFINX_DEFN)
  find_package(xsimd REQUIRED)
endif()

# Create the binding library
# pybind11 handles its own calls to target_link_libraries
pybind11_add_module(cpp MODULE
  dolfinx/wrappers/dolfinx.cpp
  dolfinx/wrappers/common.cpp
  dolfinx/wrappers/fem.cpp
  dolfinx/wrappers/geometry.cpp
  dolfinx/wrappers/graph.cpp
  dolfinx/wrappers/io.cpp
  dolfinx/wrappers/la.cpp
  dolfinx/wrappers/log.cpp
  dolfinx/wrappers/mesh.cpp
  dolfinx/wrappers/nls.cpp
  dolfinx/wrappers/refinement.cpp)

# Add strict compiler flags
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-Wall -Werror -pedantic" HAVE_PEDANTIC)
if (HAVE_PEDANTIC)
  target_compile_options(cpp PRIVATE -Wall;-Werror;-pedantic)
endif()

# In Debug mode override pybind11 symbols visibility
# Symbols must be visible to backtrace_symbols() to produce nice logs
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_options(cpp PRIVATE "-fvisibility=default")
endif()

# Add DOLFINx libraries.
target_link_libraries(cpp PRIVATE dolfinx)

# Add xsimd if the Basix C++ interface was compiled with it
if("XTENSOR_USE_XSIMD" IN_LIST BASIX_DEFN OR "XTENSOR_USE_XSIMD" IN_LIST DOLFINX_DEFN)
  target_link_libraries(cpp PRIVATE xsimd)
endif()

# Check for petsc4py
execute_process(
  COMMAND ${Python3_EXECUTABLE} -c "import petsc4py; print(petsc4py.get_include())"
  OUTPUT_VARIABLE PETSC4PY_INCLUDE_DIR
  RESULT_VARIABLE PETSC4PY_COMMAND_RESULT
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT PETSC4PY_COMMAND_RESULT)
  message(STATUS "Found petsc4py include directory at ${PETSC4PY_INCLUDE_DIR}")
  target_include_directories(cpp PRIVATE ${PETSC4PY_INCLUDE_DIR})
else()
  message(FATAL_ERROR "petsc4py could not be found.")
endif()

# Check for mpi4py
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "-c" "import mpi4py; print(mpi4py.get_include())"
  OUTPUT_VARIABLE MPI4PY_INCLUDE_DIR
  RESULT_VARIABLE MPI4PY_COMMAND_RESULT
  ERROR_QUIET  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT MPI4PY_COMMAND_RESULT)
  message(STATUS "Found mpi4py include directory at ${MPI4PY_INCLUDE_DIR}")
  target_include_directories(cpp PRIVATE ${MPI4PY_INCLUDE_DIR})
else()
  message(FATAL_ERROR "mpi4py could not be found.")
endif()
