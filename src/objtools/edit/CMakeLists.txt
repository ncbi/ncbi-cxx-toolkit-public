#############################################################################
# $Id$
#############################################################################

NCBI_add_library(edit)
NCBI_add_subdirectory(protein_match unit_test)

# Include projects from this directory
#include(CMakeLists.edit.lib.txt)

# Recurse subdirectories
#add_subdirectory(unit_test )
