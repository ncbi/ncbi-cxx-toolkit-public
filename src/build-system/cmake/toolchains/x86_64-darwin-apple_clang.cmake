#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper
##  AppleClang build settings

set(NCBI_PTBCFG_FLAGS_DEFINED YES)
include_guard(GLOBAL)

set(CMAKE_C_FLAGS_INIT              "-msse4.2 -gdwarf-4")
set(CMAKE_C_FLAGS_DEBUG_INIT        "-ggdb3 -O0 -D_DEBUG")
set(CMAKE_C_FLAGS_RELEASE_INIT      "-ggdb1 -O3 -DNDEBUG")

set(CMAKE_CXX_FLAGS_INIT            "-msse4.2 -gdwarf-4")
set(CMAKE_CXX_FLAGS_DEBUG_INIT      "-ggdb3 -O0 -D_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE_INIT    "-ggdb1 -O3 -DNDEBUG")

set(CMAKE_EXE_LINKER_FLAGS_INIT     "")
set(CMAKE_SHARED_LINKER_FLAGS_INIT  "")
