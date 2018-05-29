#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(test grid)
NCBI_add_app(
  test_nsstorage test_ic_client test_netcache_api 
  test_json_over_uttp test_compound_id test_netservice_params
)

if(OFF)
# Include projects from this directory
include(CMakeLists.test_nsstorage.app.txt)
include(CMakeLists.test_ic_client.app.txt)
include(CMakeLists.test_netcache_api.app.txt)
include(CMakeLists.test_json_over_uttp.app.txt)
include(CMakeLists.test_compound_id.app.txt)
endif()
