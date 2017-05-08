##############################################################################
# 
#

# Include projects from this directory
include(CMakeLists.ncbi_xreader.lib.txt)
include(CMakeLists.ncbi_xloader_genbank.lib.txt)

# Recurse subdirectories
add_subdirectory(cache)
add_subdirectory(pubseq)
add_subdirectory(id2)
add_subdirectory(id1)
add_subdirectory(pubseq2)
add_subdirectory(gicache)
add_subdirectory(test )
