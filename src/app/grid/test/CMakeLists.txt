#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(test)
NCBI_add_app(
  test_gridclient_stress test_grid_worker
  test_netschedule_client test_netschedule_node
  test_netschedule_stress
)

if(OFF)
# Include projects from this directory
include(CMakeLists.test_gridclient_stress.app.txt)
include(CMakeLists.test_grid_worker.app.txt)
include(CMakeLists.test_netschedule_client.app.txt)
include(CMakeLists.test_netschedule_node.app.txt)
include(CMakeLists.test_netschedule_stress.app.txt)
endif()
