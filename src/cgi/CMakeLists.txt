#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(web)
NCBI_add_library(cgi fcgi)
NCBI_add_subdirectory(test)

if(OFF)
# Include projects from this directory
include(CMakeLists.cgi.lib.txt)
if (FastCGI_FOUND)
    include(CMakeLists.fcgi.lib.txt)
endif()

# Recurse subdirectories
add_subdirectory(test )
endif()
