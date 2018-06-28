#############################################################################
# $Id$
#############################################################################

NCBI_add_definitions(HAVE_CONFIG_H=1)
NCBI_uses_toolkit_libraries(tds_ftds95)
NCBI_project_watchers(ucko satskyse)

NCBI_add_app(
  test-tds95 t0001 t0002 t0003 t0004 t0005 t0006 t0007 t0008
  dynamic1 convert dataread utf8_1 utf8_2 utf8_3 numeric iconv_fread
  toodynamic challenge readconf charconv nulls collations
)


if(OFF)
add_definitions(-DHAVE_CONFIG_H=1 -DNEED_FREETDS_SRCDIR)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# Include projects from this directory
include(CMakeLists.test-tds95.app.txt)
include(CMakeLists.t0001.app.txt)
include(CMakeLists.t0002.app.txt)
include(CMakeLists.t0003.app.txt)
include(CMakeLists.t0004.app.txt)
include(CMakeLists.t0005.app.txt)
include(CMakeLists.t0006.app.txt)
include(CMakeLists.t0007.app.txt)
include(CMakeLists.t0008.app.txt)
include(CMakeLists.dynamic1.app.txt)
include(CMakeLists.convert.app.txt)
include(CMakeLists.dataread.app.txt)
include(CMakeLists.utf8_1.app.txt)
include(CMakeLists.utf8_2.app.txt)
include(CMakeLists.utf8_3.app.txt)
include(CMakeLists.numeric.app.txt)
include(CMakeLists.iconv_fread.app.txt)
include(CMakeLists.toodynamic.app.txt)
include(CMakeLists.challenge.app.txt)
include(CMakeLists.readconf.app.txt)
include(CMakeLists.charconv.app.txt)
include(CMakeLists.nulls.app.txt)
include(CMakeLists.collations.app.txt)
endif()
