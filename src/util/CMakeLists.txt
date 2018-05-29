#############################################################################
# $Id$
#############################################################################
# 
#

NCBI_add_library(util)
NCBI_add_subdirectory(
  regexp xregexp compress diff image tables creaders
  sequtil bitset qparse lmdb lmdbxx test demo
)

if(OFF)
# Include projects from this directory
include(CMakeLists.util.lib.txt)

# Recurse subdirectories
add_subdirectory(regexp)
add_subdirectory(xregexp)
add_subdirectory(compress)
add_subdirectory(diff)
add_subdirectory(image)
add_subdirectory(tables)
add_subdirectory(creaders)
add_subdirectory(sequtil)
add_subdirectory(bitset)
add_subdirectory(qparse)
add_subdirectory(lmdbxx)
add_subdirectory(test )
add_subdirectory(demo )
endif()
