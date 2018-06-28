#############################################################################
# $Id$
#############################################################################

NCBI_add_library(alnmgr)
NCBI_add_subdirectory(demo unit_test)

if(OFF)
# Include projects from this directory
include(CMakeLists.alnmgr.lib.txt)

# Recurse subdirectories
add_subdirectory(demo )
add_subdirectory(unit_test )
endif()
