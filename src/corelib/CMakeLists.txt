##############################################################################
# 
#

# Include projects from this directory
include(CMakeLists.corelib.lib.txt)
include(CMakeLists.test_mt.lib.txt)
include(CMakeLists.test_boost.lib.txt)

# Recurse subdirectories
add_subdirectory(test )
