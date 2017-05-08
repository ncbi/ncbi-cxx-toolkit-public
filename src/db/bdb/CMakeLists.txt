##############################################################################
# 
#

# Include projects from this directory
include(CMakeLists.bdb.lib.txt)
include(CMakeLists.ncbi_xcache_bdb.lib.txt)

add_subdirectory(cache_verify)
add_subdirectory(demo)
add_subdirectory(dumper)
add_subdirectory(test)
add_subdirectory(test_split)
add_subdirectory(test_vmerge)
