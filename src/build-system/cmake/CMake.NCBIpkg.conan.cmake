#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI C++ Toolkit Conan package adapter
##  it is used when the Toolkit is built and installed as Conan package
##    Author: Andrei Gourianov, gouriano@ncbi
##

set(___silent ${CONAN_CMAKE_SILENT_OUTPUT})
set(CONAN_CMAKE_SILENT_OUTPUT TRUE)
conan_define_targets()
set(CONAN_CMAKE_SILENT_OUTPUT ${___silent})
include(${CMAKE_CURRENT_LIST_DIR}/../../ncbi-cpp-toolkit.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/CMake.NCBIpkg.codegen.cmake)
