#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(grid)
NCBI_add_library(xconnserv ncbi_xcache_netcache ncbi_xblobstorage_netcache)
NCBI_add_subdirectory(test)

if(OFF)
# Include projects from this directory
include(CMakeLists.xconnserv.lib.txt)
include(CMakeLists.ncbi_xcache_netcache.lib.txt)
include(CMakeLists.ncbi_xblobstorage_netcache.lib.txt)

# Recurse subdirectories
add_subdirectory(test )
endif()
