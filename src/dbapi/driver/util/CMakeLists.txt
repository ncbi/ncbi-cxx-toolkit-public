#############################################################################
# $Id$
#############################################################################


NCBI_add_library(dbapi_util_blobstore)
NCBI_add_app(blobreader blobwriter)
NCBI_add_subdirectory(samples)


if(OFF)
# Include projects from this directory
include(CMakeLists.dbapi_util_blobstore.lib.txt)
include(CMakeLists.blobreader.app.txt)
include(CMakeLists.blobwriter.app.txt)

# Recurse subdirectories
add_subdirectory(samples)
endif()
