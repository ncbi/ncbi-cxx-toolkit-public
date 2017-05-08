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
