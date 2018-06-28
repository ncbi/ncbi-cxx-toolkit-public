#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(test)
NCBI_add_app(sdbapi_unit_test mirror_test sdbapi_test_mt_pooling)


# Include projects from this directory
#include(CMakeLists.sdbapi_unit_test.app.txt)
#include(CMakeLists.mirror_test.app.txt)

