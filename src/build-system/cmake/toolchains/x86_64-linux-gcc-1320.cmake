#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper
##  GCC 13.2.0 toolchain

set(NCBI_PTBCFG_FLAGS_DEFINED YES)
include_guard(GLOBAL)

set(CMAKE_C_COMPILER "/opt/ncbi/gcc/13.2.0/bin/gcc")
set(CMAKE_CXX_COMPILER "/opt/ncbi/gcc/13.2.0/bin/g++")

set(CMAKE_C_FLAGS_INIT         "-gdwarf-4 -Wall -Wno-format-y2k")
set(CMAKE_C_FLAGS_DEBUG        "-ggdb3 -O0")
set(CMAKE_C_FLAGS_RELEASE      "-ggdb1 -O3")
set(CMAKE_C_FLAGS_RELWITHDEBINFO  "-ggdb3 -O3")

set(CMAKE_CXX_FLAGS_INIT       "-gdwarf-4 -Wall -Wno-format-y2k")
set(CMAKE_CXX_FLAGS_DEBUG      "-ggdb3 -O0")
set(CMAKE_CXX_FLAGS_RELEASE    "-ggdb1 -O3")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "-ggdb3 -O3")

set(CMAKE_EXE_LINKER_FLAGS_INIT     "-Wl,--enable-new-dtags  -Wl,--as-needed")
set(CMAKE_SHARED_LINKER_FLAGS_INIT  "-Wl,--no-undefined  -Wl,--as-needed")

#----------------------------------------------------------------------------
set(NCBI_COMPILER_FLAGS_SSE       "-msse4.2")

set(NCBI_COMPILER_FLAGS_COVERAGE  "--coverage")
set(NCBI_LINKER_FLAGS_COVERAGE     "--coverage")

set(NCBI_COMPILER_FLAGS_MAXDEBUG  "-fsanitize=address -fstack-check")
set(NCBI_LINKER_FLAGS_MAXDEBUG   "-fsanitize=address")

set(NCBI_LINKER_FLAGS_STATICCOMPONENTS "-static-libgcc -static-libstdc++")
