#############################################################################
# $Id$
#############################################################################

NCBI_add_library(tds100)

if(OFF)
# Include projects from this directory
include(CMakeLists.tds100.lib.txt)

# Recurse subdirectories
add_subdirectory(unittests)
endif()
