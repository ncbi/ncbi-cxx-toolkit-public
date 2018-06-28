#############################################################################
# $Id$
#############################################################################

NCBI_add_library(ncbi_xloader_blastdb ncbi_xloader_blastdb_rmt)
NCBI_add_subdirectory(unit_test)

# Include projects from this directory
#include(CMakeLists.ncbi_xloader_blastdb.lib.txt)
#include(CMakeLists.ncbi_xloader_blastdb_rmt.lib.txt)

# Recurse subdirectories
#add_subdirectory(unit_test )
