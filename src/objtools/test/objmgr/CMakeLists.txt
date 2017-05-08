##############################################################################
# 
#

# Include projects from this directory
include(CMakeLists.test_objmgr_data.app.txt)
include(CMakeLists.test_objmgr_data_mt.app.txt)
include(CMakeLists.test_objmgr_mem.app.txt)
include(CMakeLists.test_seqvector_ci.app.txt)
include(CMakeLists.seqvec_bench.app.txt)
include(CMakeLists.test_annot_ci.app.txt)
include(CMakeLists.test_edit_saver.app.txt)
include(CMakeLists.id_unit_test.app.txt)
include(CMakeLists.test_feat_tree.app.txt)
include(CMakeLists.feat_unit_test.app.txt)
include(CMakeLists.test_seq_entry_ci.app.txt)

# Recurse subdirectories
add_subdirectory(util)
