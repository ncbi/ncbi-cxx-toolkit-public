##############################################################################
# 
#

include_directories(SYSTEM ${BOOST_INCLUDE})

# Include projects from this directory
include(CMakeLists.unit_test_seq_translator.app.txt)
include(CMakeLists.unit_test_fasta_ostream.app.txt)
include(CMakeLists.unit_test_mol_wt.app.txt)
include(CMakeLists.unit_test_seq_loc_util.app.txt)
include(CMakeLists.unit_test_defline.app.txt)
include(CMakeLists.unit_test_bioseqgaps_ci.app.txt)
include(CMakeLists.unit_test_obj_sniff.app.txt)

