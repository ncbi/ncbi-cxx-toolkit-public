#############################################################################
# $Id$
#############################################################################


NCBI_requires(Boost.Spirit)
NCBI_add_library(hgvs_parser)
NCBI_add_subdirectory(test)

# 
#

# Include projects from this directory
#include(CMakeLists.hgvs_parser.lib.txt)

# Recurse subdirectories
#add_subdirectory(test )
