#############################################################################
# $Id$
#############################################################################


NCBI_add_library(ncbi_xdbapi_ftds100)
NCBI_add_subdirectory(samples)

if(OFF)
# Include projects from this directory
include(CMakeLists.ncbi_xdbapi_ftds100.lib.txt)

# Recurse subdirectories
add_subdirectory(samples)
endif()
