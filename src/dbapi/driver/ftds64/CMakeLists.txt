#############################################################################
# $Id$
#############################################################################

NCBI_add_include_directories(${FTDS64_INCLUDE})
NCBI_add_subdirectory(freetds ctlib)

# Include projects from this directory

# Recurse subdirectories
#add_subdirectory(freetds)
#add_subdirectory(ctlib)
