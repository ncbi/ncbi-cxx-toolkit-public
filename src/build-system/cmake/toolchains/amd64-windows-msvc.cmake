#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper
##  MSVC build settings

set(NCBI_PTBCFG_FLAGS_DEFINED YES)
include_guard(GLOBAL)

set(CMAKE_C_FLAGS_INIT      "/W3 /MP /Zi")
set(CMAKE_C_FLAGS_DEBUG     "/Od /RTC1")
set(CMAKE_C_FLAGS_RELEASE   "/O2 /Ob1")

set(CMAKE_CXX_FLAGS_INIT    "/W3 /MP /Zi")
set(CMAKE_CXX_FLAGS_DEBUG   "/Od /RTC1")
set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob1")

set(CMAKE_EXE_LINKER_FLAGS_INIT    "/INCREMENTAL:NO")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "/INCREMENTAL:NO")
set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO    "/DEBUG:FULL")
set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "/DEBUG:FULL")
