#############################################################################
# $Id$
#############################################################################

NCBI_add_library(ctools)
NCBI_add_subdirectory(asn test ctransition)

if(OFF)
# 
#

# Everything here requires the C toolkit
link_directories(${NCBI_C_LIBPATH})
include_directories(SYSTEM ${NCBI_C_INCLUDE})

# Include projects from this directory
include(CMakeLists.ctools.lib.txt)

# Recurse subdirectories
add_subdirectory(asn)
add_subdirectory(ctransition)
add_subdirectory(test)
endif()
