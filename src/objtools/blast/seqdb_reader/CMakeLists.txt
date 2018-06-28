#############################################################################
# $Id$
#############################################################################

NCBI_add_library(seqdb)
NCBI_add_subdirectory(demo test)

if(OFF)
# Include projects from this directory
include(CMakeLists.seqdb.lib.txt)

# Recurse subdirectories
add_subdirectory(demo )
add_subdirectory(test )
endif()
