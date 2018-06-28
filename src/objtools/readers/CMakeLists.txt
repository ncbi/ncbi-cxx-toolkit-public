#############################################################################
# $Id$
#############################################################################

NCBI_add_library(xobjread xobjreadex)
NCBI_add_subdirectory(app test unit_test)

if(OFF)
# Include projects from this directory
include(CMakeLists.xobjread.lib.txt)
include(CMakeLists.xobjreadex.lib.txt)

# Recurse subdirectories
add_subdirectory(app)
add_subdirectory(test )
add_subdirectory(unit_test )
endif()
