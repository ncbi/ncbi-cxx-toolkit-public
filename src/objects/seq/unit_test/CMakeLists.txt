#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(test)
NCBI_requires(Boost.Test.Included)
NCBI_add_app(
  ecnum_unit_test
  mapper_unit_test
  seq_c_compat_unit_test
  seq_id_unit_test
  seq_loc_unit_test
  seqalign_unit_test 
  seqfeat_unit_test 
  seqfeatdata_unit_test
  unit_test_so_map
)

if(OFF)
# Include projects from this directory
include(CMakeLists.seq_id_unit_test.app.txt)
include(CMakeLists.seqalign_unit_test.app.txt)
include(CMakeLists.mapper_unit_test.app.txt)
include(CMakeLists.ecnum_unit_test.app.txt)
include(CMakeLists.seqfeatdata_unit_test.app.txt)
include(CMakeLists.seqfeat_unit_test.app.txt)
include(CMakeLists.seq_loc_unit_test.app.txt)

if (HAVE_NCBI_C)

include(CMakeLists.seq_c_compat_unit_test.app.txt)

endif()
endif()
