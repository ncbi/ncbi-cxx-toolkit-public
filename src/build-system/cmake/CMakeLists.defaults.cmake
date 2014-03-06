set(top_srcdir   ${NCBI_TREE_ROOT})
set(build_root   ${NCBI_BUILD_ROOT}/..)
set(builddir     ${NCBI_BUILD_ROOT})
set(includedir0  ${top_srcdir}/include)
set(includedir   ${includedir0})
set(incdir       ${build_root}/inc)
set(incinternal  ${includedir0}/internal)
set(buildconf    GCC401-DebugMT64)
set(buildconf0   GCC401-DebugMT)

set(CONF_CXXFLAGS   -Wall -Wno-format-y2k  -pthread -fPIC  -gdwarf-3)
set(CONF_CPPFLAGS  -D_DEBUG -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
                   -D_MT -D_REENTRANT -D_THREAD_SAFE)
set(ORIG_CPPFLAGS  ${CONF_CXXFLAGS} ${CONF_CPPFLAGS})

set(NCBI_TOOLS_ROOT $ENV{NCBI})

#set (ARCHIVE_OUTPUT_DIRECTORY ${NCBI_BUILD_ROOT}/../lib)
#set (LIBRARY_OUTPUT_DIRECTORY ${NCBI_BUILD_ROOT}/../lib)
#set (RUNTIME_OUTPUT_DIRECTORY ${NCBI_BUILD_ROOT}/../bin)
#set (PDB_OUTPUT_DIRECTORY     ${NCBI_BUILD_ROOT}/../bin)

#set (LIBRARY_OUTPUT_PATH     ${NCBI_BUILD_ROOT}/../lib)
#set (EXECUTABLE_OUTPUT_PATH  ${NCBI_BUILD_ROOT}/../bin)
set (LIBRARY_OUTPUT_PATH     ${NCBI_CMAKE_ROOT}/lib)
set (EXECUTABLE_OUTPUT_PATH  ${NCBI_CMAKE_ROOT}/bin)

set (NCBI_BUILD_BIN ${EXECUTABLE_OUTPUT_PATH})

#set (NCBI_DATATOOL $NCBI/bin/datatool)
set (NCBI_DATATOOL /netopt/ncbi_tools64/bin/datatool)

ENABLE_TESTING()

include(CMakeChecks)
include_directories(${incdir} ${includedir0} ${incinternal})


