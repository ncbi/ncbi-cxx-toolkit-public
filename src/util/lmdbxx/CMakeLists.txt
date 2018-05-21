#############################################################################
# $Id$
#############################################################################

NCBI_requires(LMDB)
NCBI_add_app(lmdbxx_sample)

# Include projects from this directory
# temporaty disabled until CXX-9344 is done
#include(CMakeLists.lmdbxx_sample.app.txt)
