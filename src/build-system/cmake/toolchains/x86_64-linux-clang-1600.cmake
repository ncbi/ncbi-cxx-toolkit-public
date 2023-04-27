#############################################################################
# $
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper
##  Clang 16.0.0 toolchain

set(NCBI_PTBCFG_FLAGS_DEFINED YES)
include_guard(GLOBAL)

set(CMAKE_C_COMPILER "/usr/local/llvm/16.0.0/bin/clang")
set(CMAKE_CXX_COMPILER "/usr/local/llvm/16.0.0/bin/clang++")

set(CMAKE_C_FLAGS
    "-isystem /opt/ncbi/gcc/7.3.0/include/c++/7.3.0 \
    -isystem /opt/ncbi/gcc/7.3.0/include/c++/7.3.0/x86_64-redhat-linux-gnu  \
    -isystem /opt/ncbi/gcc/7.3.0/include/c++/7.3.0/backward \
    -gdwarf-4"
)
set(CMAKE_C_FLAGS_DEBUG   "-ggdb3 -O0")
set(CMAKE_C_FLAGS_RELEASE "-ggdb1 -O3")

set(CMAKE_CXX_FLAGS
    "-isystem /opt/ncbi/gcc/7.3.0/include/c++/7.3.0 \
    -isystem /opt/ncbi/gcc/7.3.0/include/c++/7.3.0/x86_64-redhat-linux-gnu \
    -isystem /opt/ncbi/gcc/7.3.0/include/c++/7.3.0/backward \
    -gdwarf-4"
)
set(CMAKE_CXX_FLAGS_DEBUG   "-ggdb3 -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-ggdb1 -O3")

set(CMAKE_EXE_LINKER_FLAGS
    "-L/opt/ncbi/gcc/7.3.0/lib/gcc/x86_64-redhat-linux-gnu/7.3.0 \
    -B/opt/ncbi/gcc/7.3.0/lib/gcc/x86_64-redhat-linux-gnu/7.3.0 \
    -L/opt/ncbi/gcc/7.3.0/lib64 \
    -Wl,-rpath,/opt/ncbi/gcc/7.3.0/lib64 \
    -Wl,--enable-new-dtags"
)

set(CMAKE_SHARED_LINKER_FLAGS
    "-L/opt/ncbi/gcc/7.3.0/lib/gcc/x86_64-redhat-linux-gnu/7.3.0 \
    -B/opt/ncbi/gcc/7.3.0/lib/gcc/x86_64-redhat-linux-gnu/7.3.0 \
    -L/opt/ncbi/gcc/7.3.0/lib64 \
    -Wl,-rpath,/opt/ncbi/gcc/7.3.0/lib64 \
    -Wl,--no-undefined"
)
