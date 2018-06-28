#############################################################################
# $Id$
#############################################################################

NCBI_add_library(ctlib100)

if(OFF)
# Include projects from this directory
include(CMakeLists.ctlib100.lib.txt)

# Recurse subdirectories
add_subdirectory(unittests)
endif()
