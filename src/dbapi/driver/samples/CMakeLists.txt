#############################################################################
# $Id$
#############################################################################


NCBI_project_watchers(ucko satskyse)
NCBI_add_library(dbapi_sample_base)
NCBI_add_app(lang_query dbapi_driver_check)
NCBI_add_subdirectory(
  dbapi_bcp dbapi_cursor dbapi_query dbapi_send_data dbapi_testspeed dbapi_conn_policy
)

if(OFF)
##############################################################################
# 
#

# Include projects from this directory
include(CMakeLists.dbapi_sample_base.lib.txt)
include(CMakeLists.lang_query.app.txt)
include(CMakeLists.dbapi_driver_check.app.txt)

# Recurse subdirectories
add_subdirectory(dbapi_bcp)
add_subdirectory(dbapi_cursor)
add_subdirectory(dbapi_query)
add_subdirectory(dbapi_send_data)
add_subdirectory(dbapi_testspeed)
add_subdirectory(dbapi_conn_policy)
endif()
