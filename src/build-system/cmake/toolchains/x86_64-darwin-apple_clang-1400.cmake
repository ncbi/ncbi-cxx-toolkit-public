#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper
##  AppleClang 14.0 build settings

include_guard(GLOBAL)

set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_VERSION 21.6.0)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER "/usr/bin/clang")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")

include(${CMAKE_CURRENT_LIST_DIR}/x86_64-darwin-apple_clang.cmake)
