#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(test)
NCBI_add_app(
  test_reader_id1 test_reader_pubseq test_reader_gicache
  test_objmgr_gbloader test_objmgr_gbloader_mt
  test_bulkinfo test_bulkinfo_mt
)

if(OFF)
# Include projects from this directory
include(CMakeLists.test_reader_id1.app.txt)
include(CMakeLists.test_reader_pubseq.app.txt)
#include(CMakeLists.test_reader_gicache.app.txt)
include(CMakeLists.test_objmgr_gbloader.app.txt)
include(CMakeLists.test_objmgr_gbloader_mt.app.txt)
include(CMakeLists.test_bulkinfo.app.txt)
include(CMakeLists.test_bulkinfo_mt.app.txt)
endif()
