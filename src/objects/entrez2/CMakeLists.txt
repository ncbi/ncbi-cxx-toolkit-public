#############################################################################
# $Id$
#############################################################################

NCBI_add_library(entrez2 entrez2cli)
NCBI_add_subdirectory(demo)

# Include projects from this directory
#include(CMakeLists.entrez2cli.lib.txt)
#include(CMakeLists.entrez2.asn.txt)

# Recurse subdirectories
#add_subdirectory(demo )
