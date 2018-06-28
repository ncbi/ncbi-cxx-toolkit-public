#############################################################################
# $Id$
#############################################################################

NCBI_project_watchers(ucko satskyse)
NCBI_add_library(sdbapi)
NCBI_add_subdirectory(test)

# Include projects from this directory
#include(CMakeLists.sdbapi.lib.txt)

# Recurse subdirectories
#add_subdirectory(test )
