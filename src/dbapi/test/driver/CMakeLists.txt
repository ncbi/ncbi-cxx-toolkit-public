#############################################################################
# $Id$
#############################################################################

NCBI_add_app(
  ctl_sp_databases ctl_sp_databases_ftds
  dbapi_driver_test_ftds_ctlib dbapi_driver_test_ctlib
)

if(OFF)

if (SYBASE_FOUND)
  # Include projects from this directory
  include(CMakeLists.ctl_sp_databases.app.txt)
  include(CMakeLists.ctl_sp_databases_ftds.app.txt)
  include(CMakeLists.dbapi_driver_test_ctlib.app.txt)
  include(CMakeLists.dbapi_driver_test_ftds_ctlib.app.txt)
endif (SYBASE_FOUND)

# FIXME
#include(CMakeLists.dbapi_driver_test_ctlib.app.txt)
endif()
