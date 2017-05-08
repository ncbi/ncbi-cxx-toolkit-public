##############################################################################
# 
#

if (SYBASE_FOUND)

  # Include projects from this directory
  include(CMakeLists.ncbi_xdbapi_ctlib.lib.txt)

  # Recurse subdirectories
  add_subdirectory(samples)

endif (SYBASE_FOUND)

