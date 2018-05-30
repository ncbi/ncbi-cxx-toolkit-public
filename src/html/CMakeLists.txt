#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(web)
NCBI_add_library(html)
NCBI_add_subdirectory(test demo)

if(OFF)
# 
#

# Include projects from this directory
include(CMakeLists.html.lib.txt)

# Recurse subdirectories
add_subdirectory(test )
add_subdirectory(demo )
endif()
