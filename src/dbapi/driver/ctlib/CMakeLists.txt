#############################################################################
# $Id$
#############################################################################

NCBI_requires(Sybase)
NCBI_add_library(ncbi_xdbapi_ctlib)
NCBI_add_subdirectory(samples)


if(OFF)
if (SYBASE_FOUND)

  # Include projects from this directory
  include(CMakeLists.ncbi_xdbapi_ctlib.lib.txt)

  # Recurse subdirectories
  add_subdirectory(samples)

endif (SYBASE_FOUND)
endif()

