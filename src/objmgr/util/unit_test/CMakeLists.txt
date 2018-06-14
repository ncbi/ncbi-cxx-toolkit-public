#############################################################################
# $Id$
#############################################################################


NCBI_project_tags(test)
NCBI_add_app(
  unit_test_seq_translator unit_test_fasta_ostream
  unit_test_mol_wt unit_test_seq_loc_util unit_test_defline
  unit_test_bioseqgaps_ci unit_test_obj_sniff unit_test_get_label 
)

if(OFF)


# Include projects from this directory
include(CMakeLists.unit_test_seq_translator.app.txt)
include(CMakeLists.unit_test_fasta_ostream.app.txt)
include(CMakeLists.unit_test_mol_wt.app.txt)
include(CMakeLists.unit_test_seq_loc_util.app.txt)
include(CMakeLists.unit_test_defline.app.txt)
include(CMakeLists.unit_test_bioseqgaps_ci.app.txt)
include(CMakeLists.unit_test_obj_sniff.app.txt)
endif()
