#############################################################################
# $Id$
#############################################################################

NCBI_add_library(ctransition ctransition_nlmzip)
NCBI_add_subdirectory(test)

if(OFF)
include(CMakeLists.ctransition.lib.txt)

add_subdirectory(test)
endif()
