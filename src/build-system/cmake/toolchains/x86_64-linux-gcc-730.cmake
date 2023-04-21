#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper
##  GCC 7.3.0 build settings

include_guard(GLOBAL)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_VERSION 3.10.0)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER "/opt/ncbi/gcc/7.3.0/bin/gcc")
set(CMAKE_CXX_COMPILER "/opt/ncbi/gcc/7.3.0/bin/g++")

include(${CMAKE_CURRENT_LIST_DIR}/x86_64-linux-gcc.cmake)
