#############################################################################
# $Id$
#############################################################################

NCBI_add_library(xalgophytree)
NCBI_add_subdirectory(fastme test unit_test phytree_format)

if(OFF)
# Include projects from this directory
include(CMakeLists.xalgophytree.lib.txt)

# Recurse subdirectories
add_subdirectory(fastme)
add_subdirectory(test )
add_subdirectory(unit_test )
add_subdirectory(phytree_format)
endif()
