#############################################################################
# $Id$
#############################################################################

NCBI_add_definitions(HAVE_CONFIG_H=1 NEED_FREETDS_SRCDIR)
NCBI_uses_toolkit_libraries(sybdb_ftds100)
NCBI_project_watchers(ucko satskyse)

NCBI_add_app(
  test-db100 t0001 t0002 t0003 t0004 t0005 t0006 t0007 t0008 t0009
  t0011 t0012 t0013 t0014 t0015 t0016 t0017 t0018 t0019 t0020 t0021
  t0022 t0023 rpc dbmorecmds bcp thread text_buffer done_handling
  timeout hang null null2 setnull numeric pending cancel spid bcp_getl
)


if(OFF)
include_directories(SYSTEM ${BOOST_INCLUDE})

add_definitions(-DHAVE_CONFIG_H=1 -DNEED_FREETDS_SRCDIR)

# Include projects from this directory
include(CMakeLists.test-db100.app.txt)
include(CMakeLists.t0001.app.txt)
include(CMakeLists.t0002.app.txt)
include(CMakeLists.t0003.app.txt)
include(CMakeLists.t0004.app.txt)
include(CMakeLists.t0005.app.txt)
include(CMakeLists.t0006.app.txt)
include(CMakeLists.t0007.app.txt)
include(CMakeLists.t0008.app.txt)
include(CMakeLists.t0009.app.txt)
include(CMakeLists.t0011.app.txt)
include(CMakeLists.t0012.app.txt)
include(CMakeLists.t0013.app.txt)
include(CMakeLists.t0014.app.txt)
include(CMakeLists.t0015.app.txt)
include(CMakeLists.t0016.app.txt)
include(CMakeLists.t0017.app.txt)
include(CMakeLists.t0018.app.txt)
include(CMakeLists.t0019.app.txt)
include(CMakeLists.t0020.app.txt)
include(CMakeLists.t0021.app.txt)
include(CMakeLists.t0022.app.txt)
include(CMakeLists.t0023.app.txt)
include(CMakeLists.rpc.app.txt)
include(CMakeLists.dbmorecmds.app.txt)
include(CMakeLists.bcp.app.txt)
include(CMakeLists.thread.app.txt)
include(CMakeLists.text_buffer.app.txt)
include(CMakeLists.done_handling.app.txt)
include(CMakeLists.timeout.app.txt)
include(CMakeLists.hang.app.txt)
include(CMakeLists.null.app.txt)
include(CMakeLists.null2.app.txt)
include(CMakeLists.setnull.app.txt)
include(CMakeLists.numeric.app.txt)
include(CMakeLists.pending.app.txt)
include(CMakeLists.cancel.app.txt)
include(CMakeLists.spid.app.txt)
include(CMakeLists.bcp_getl.app.txt)
endif()
