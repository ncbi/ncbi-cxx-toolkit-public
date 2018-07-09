#############################################################################
# $Id$
#############################################################################

NCBI_add_library(xalgoseq)
NCBI_add_subdirectory(demo unit_test)

if(OFF)
# Include projects from this directory
include(CMakeLists.xalgoseq.lib.txt)

# Recurse subdirectories
add_subdirectory(demo )
add_subdirectory(unit_test )
endif()
