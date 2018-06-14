#############################################################################
# $Id$
#############################################################################


NCBI_project_tags(test)
NCBI_add_app(
  test_objmgr_basic
  test_objmgr
  test_objmgr_mt
  test_objmgr_sv
  test_seqmap_switch
  unit_test_objmgr
)
# 
#

# Include projects from this directory
#include(CMakeLists.test_objmgr_basic.app.txt)
#include(CMakeLists.test_objmgr.app.txt)
#include(CMakeLists.test_objmgr_mt.app.txt)
#include(CMakeLists.test_objmgr_sv.app.txt)
#include(CMakeLists.test_seqmap_switch.app.txt)

