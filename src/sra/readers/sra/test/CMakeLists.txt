#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(test)
NCBI_add_app(sra_test vdb_test wgs_test csra_test_mt graph_test snp_test)


if(OFF)
# Include projects from this directory
include(CMakeLists.sra_test.app.txt)
include(CMakeLists.vdb_test.app.txt)
include(CMakeLists.wgs_test.app.txt)
include(CMakeLists.csra_test_mt.app.txt)
include(CMakeLists.graph_test.app.txt)
include(CMakeLists.snp_test.app.txt)
endif()
