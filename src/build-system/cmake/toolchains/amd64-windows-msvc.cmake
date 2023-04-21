#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper
##  MSVC build settings

set(NCBI_PTBCFG_FLAGS_DEFINED YES)
include_guard(GLOBAL)

set(CMAKE_C_FLAGS            "/DWIN32 /D_WINDOWS /W3 /MP /Zi")
set(CMAKE_C_FLAGS_DEBUGDLL   "/MDd /Od /RTC1 /D_DEBUG")
set(CMAKE_C_FLAGS_DEBUGMT    "/MTd /Od /RTC1 /D_DEBUG")
set(CMAKE_C_FLAGS_RELEASEDLL "/MD  /O2 /Ob1 /DNDEBUG")
set(CMAKE_C_FLAGS_RELEASEMT  "/MT  /O2 /Ob1 /DNDEBUG")

set(CMAKE_CXX_FLAGS            "/DWIN32 /D_WINDOWS /W3 /GR /EHsc /MP /Zi")
set(CMAKE_CXX_FLAGS_DEBUGDLL   "/MDd /Od /RTC1 /D_DEBUG")
set(CMAKE_CXX_FLAGS_DEBUGMT    "/MTd /Od /RTC1 /D_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASEDLL "/MD  /O2 /Ob1 /DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASEMT  "/MT  /O2 /Ob1 /DNDEBUG")

set(CMAKE_EXE_LINKER_FLAGS            "/machine:x64 /INCREMENTAL:NO")
set(CMAKE_EXE_LINKER_FLAGS_DEBUGDLL   "/DEBUG")
set(CMAKE_EXE_LINKER_FLAGS_DEBUGMT    "/DEBUG")
set(CMAKE_EXE_LINKER_FLAGS_RELEASEDLL "")
set(CMAKE_EXE_LINKER_FLAGS_RELEASEMT  "")

set(CMAKE_SHARED_LINKER_FLAGS            "/machine:x64 /INCREMENTAL:NO")
set(CMAKE_SHARED_LINKER_FLAGS_DEBUGDLL   "/DEBUG")
set(CMAKE_SHARED_LINKER_FLAGS_DEBUGMT    "/DEBUG")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASEDLL "")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASEMT  "")
