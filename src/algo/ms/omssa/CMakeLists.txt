#############################################################################
# $Id$
#############################################################################

NCBI_add_library(xomssa)
NCBI_add_app(omssacl omssamerge omssa2pepXML)
NCBI_add_subdirectory(demo)

if(OFF)
# Include projects from this directory
include(CMakeLists.xomssa.lib.txt)
include(CMakeLists.omssacl.app.txt)
include(CMakeLists.omssamerge.app.txt)
include(CMakeLists.omssa2pepXML.app.txt)

# Recurse subdirectories
add_subdirectory(demo )
endif()
