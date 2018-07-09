#############################################################################
# $Id$
#############################################################################

NCBI_add_library(ncbi_xloader_wgs id2wgs)
NCBI_add_subdirectory(test)

# Include projects from this directory
#include(CMakeLists.ncbi_xloader_wgs.lib.txt)
#include(CMakeLists.id2wgs.lib.txt)

# Recurse subdirectories
#add_subdirectory(test )
