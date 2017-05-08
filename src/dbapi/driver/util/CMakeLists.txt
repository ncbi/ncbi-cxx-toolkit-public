##############################################################################
# 
#

# Include projects from this directory
include(CMakeLists.dbapi_util_blobstore.lib.txt)
include(CMakeLists.blobreader.app.txt)
include(CMakeLists.blobwriter.app.txt)

# Recurse subdirectories
add_subdirectory(samples)
