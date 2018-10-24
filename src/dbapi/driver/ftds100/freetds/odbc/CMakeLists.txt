#############################################################################
# $Id$
#############################################################################

NCBI_requires(ODBC)
NCBI_add_library(odbc100)
NCBI_add_subdirectory(unittests)

# Include projects from this directory
#include(CMakeLists.odbc100.lib.txt)

# Recurse subdirectories
#add_subdirectory(unittests)
