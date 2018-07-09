#############################################################################
# $Id$
#############################################################################

NCBI_add_subdirectory(util nw splign contig_assembly prosplign mergetree ngalign)

##############################################################################
# 
#

if(OFF)
# Include projects from this directory

# Recurse subdirectories
add_subdirectory(util)
add_subdirectory(nw)
add_subdirectory(splign)
add_subdirectory(contig_assembly)
add_subdirectory(prosplign)
add_subdirectory(mergetree)
add_subdirectory(ngalign)
endif()
