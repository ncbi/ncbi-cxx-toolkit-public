#############################################################################
# $Id$
#############################################################################

NCBI_add_library(general)
NCBI_add_subdirectory(test unit_test)

if(OFF)
# Include projects from this directory
include(CMakeLists.general.asn.txt)

# Recurse subdirectories
add_subdirectory(test )
add_subdirectory(unit_test )
endif()
