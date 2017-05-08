##############################################################################
# 
#

# Include projects from this directory
include(CMakeLists.genome_collection.asn.txt)
include(CMakeLists.gencoll_client.asn.txt)

# Recurse subdirectories
add_subdirectory(gc_cli )
