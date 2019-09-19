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

