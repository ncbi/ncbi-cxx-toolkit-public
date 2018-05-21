#############################################################################
# $Id$
#############################################################################
# 
#

# Include projects from this directory

if (NCBI_EXPERIMENTAL_CFG)
NCBI_add_subdirectory(project_tree_builder)

else()
# Recurse subdirectories
add_subdirectory(project_tree_builder)
#add_subdirectory(helpers)
endif()
