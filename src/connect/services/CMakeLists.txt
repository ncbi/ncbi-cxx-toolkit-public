##############################################################################
# 
#

# Include projects from this directory
include(CMakeLists.xconnserv.lib.txt)
include(CMakeLists.ncbi_xcache_netcache.lib.txt)
include(CMakeLists.ncbi_xblobstorage_netcache.lib.txt)

# Recurse subdirectories
add_subdirectory(test )
