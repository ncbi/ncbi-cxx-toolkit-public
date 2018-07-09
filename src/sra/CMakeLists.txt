#############################################################################
# $Id$
#############################################################################

NCBI_requires(VDB)
NCBI_add_subdirectory(
  readers app data_loaders
)

if(OFF)
# Include projects from this directory

# Recurse subdirectories

if (VDB_FOUND)
  add_subdirectory(readers)
  add_subdirectory(data_loaders)
  add_subdirectory(app)
endif (VDB_FOUND)
endif()
