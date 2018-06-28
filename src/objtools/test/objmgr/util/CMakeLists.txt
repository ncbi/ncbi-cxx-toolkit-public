#############################################################################
# $Id$
#############################################################################

NCBI_add_app(
  test_title test_relloc test_feat_overlap unit_test_seq_trimmer
  unit_test_seq
)

if(OFF)
# Include projects from this directory
include(CMakeLists.test_title.app.txt)
include(CMakeLists.test_relloc.app.txt)
include(CMakeLists.test_feat_overlap.app.txt)
include(CMakeLists.unit_test_seq_trimmer.app.txt)
include(CMakeLists.unit_test_seq.app.txt)
endif()
