#############################################################################
# $Id$
#############################################################################

NCBI_requires(Linux)
NCBI_add_app(netstoraged)
NCBI_add_subdirectory(utils)

if (OFF)
if (UNIX AND NOT APPLE)

  # Include projects from this directory
  include(CMakeLists.netstoraged.app.txt)

  # Recurse subdirectories
  add_subdirectory(utils)

endif ()
endif()
