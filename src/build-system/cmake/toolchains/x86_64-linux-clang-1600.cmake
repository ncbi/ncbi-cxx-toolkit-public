#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper
##  Clang 16.0.0 toolchain

set(NCBI_PTBCFG_FLAGS_DEFINED YES)
include_guard(GLOBAL)

set(CMAKE_C_COMPILER "/usr/local/llvm/16.0.0/bin/clang")
set(CMAKE_CXX_COMPILER "/usr/local/llvm/16.0.0/bin/clang++")

set(CMAKE_C_FLAGS_INIT
    "-isystem /opt/ncbi/gcc/13.2.0/include/c++/13.2.0 \
    -isystem /opt/ncbi/gcc/13.2.0/include/c++/13.2.0/x86_64-redhat-linux-gnu  \
    -isystem /opt/ncbi/gcc/13.2.0/include/c++/13.2.0/backward \
    -gdwarf-4"
)
set(CMAKE_C_FLAGS_DEBUG   "-ggdb3 -O0")
set(CMAKE_C_FLAGS_RELEASE "-ggdb1 -O3")
set(CMAKE_C_FLAGS_RELWITHDEBINFO  "-ggdb3 -O3")

set(CMAKE_CXX_FLAGS_INIT
    "-nostdinc++ -isystem /opt/ncbi/gcc/13.2.0/include/c++/13.2.0 \
    -isystem /opt/ncbi/gcc/13.2.0/include/c++/13.2.0/x86_64-redhat-linux-gnu \
    -isystem /opt/ncbi/gcc/13.2.0/include/c++/13.2.0/backward \
    -gdwarf-4"
)
set(CMAKE_CXX_FLAGS_DEBUG   "-ggdb3 -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-ggdb1 -O3")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "-ggdb3 -O3")

set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-L/opt/ncbi/gcc/13.2.0/lib/gcc/x86_64-redhat-linux-gnu/13.2.0 \
    -B/opt/ncbi/gcc/13.2.0/lib/gcc/x86_64-redhat-linux-gnu/13.2.0 \
    -L/opt/ncbi/gcc/13.2.0/lib64 \
    -Wl,-rpath,/opt/ncbi/gcc/13.2.0/lib64 \
    -Wl,--enable-new-dtags"
)

set(CMAKE_SHARED_LINKER_FLAGS_INIT
    "-L/opt/ncbi/gcc/13.2.0/lib/gcc/x86_64-redhat-linux-gnu/13.2.0 \
    -B/opt/ncbi/gcc/13.2.0/lib/gcc/x86_64-redhat-linux-gnu/13.2.0 \
    -L/opt/ncbi/gcc/13.2.0/lib64 \
    -Wl,-rpath,/opt/ncbi/gcc/13.2.0/lib64 \
    -Wl,--no-undefined"
)

#----------------------------------------------------------------------------
set(NCBI_COMPILER_FLAGS_SSE       "-msse4.2")

set(NCBI_COMPILER_FLAGS_COVERAGE  "")
set(NCBI_LINKER_FLAGS_COVERAGE     "")

set(NCBI_COMPILER_FLAGS_MAXDEBUG  "")
set(NCBI_LINKER_FLAGS_MAXDEBUG   "")

set(NCBI_LINKER_FLAGS_STATICCOMPONENTS "-static-libgcc -static-libstdc++")
