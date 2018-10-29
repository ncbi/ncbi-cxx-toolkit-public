#############################################################################
# $Id$
#############################################################################

NCBI_requires(Linux)
NCBI_add_app(netscheduled)
NCBI_add_subdirectory(test )

if (OFF)
if (BERKLEYDB_FOUND AND UNIX AND NOT APPLE)
  
  # Include projects from this directory
  include(CMakeLists.netscheduled.app.txt)

  # Recurse subdirectories
  add_subdirectory(test )

endif ()
endif()
