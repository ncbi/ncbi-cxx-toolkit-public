#############################################################################
# $Id$
#############################################################################

NCBI_add_library(validator)
NCBI_add_subdirectory(test unit_test)

# Include projects from this directory
#include(CMakeLists.validator.lib.txt)

# Recurse subdirectories
#add_subdirectory(test )
#add_subdirectory(unit_test )
