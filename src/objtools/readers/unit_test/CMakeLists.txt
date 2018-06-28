#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(test)
NCBI_add_app(
  unit_test_idmapper
  unit_test_format_guess
  unit_test_format_guess_ex
  unit_test_feature_table_reader
  unit_test_fasta_reader
  unit_test_agp_seq_entry
  unit_test_agp_converter
  unit_test_gvfreader
  unit_test_gff3reader
  unit_test_gff3reader_genbank
  unit_test_bedreader
  unit_test_wigreader
  unit_test_vcfreader
  unit_test_ucscreader
  unit_test_gtfreader
  unit_test_alnreader
  unit_test_5colftblreader
)


if(OFF)
# Include projects from this directory
include(CMakeLists.unit_test_idmapper.app.txt)
include(CMakeLists.unit_test_format_guess.app.txt)
include(CMakeLists.unit_test_format_guess_ex.app.txt)
include(CMakeLists.unit_test_feature_table_reader.app.txt)
include(CMakeLists.unit_test_fasta_reader.app.txt)
include(CMakeLists.unit_test_agp_seq_entry.app.txt)
include(CMakeLists.unit_test_agp_converter.app.txt)
include(CMakeLists.unit_test_gvfreader.app.txt)
include(CMakeLists.unit_test_gff3reader.app.txt)
include(CMakeLists.unit_test_bedreader.app.txt)
include(CMakeLists.unit_test_wigreader.app.txt)
include(CMakeLists.unit_test_vcfreader.app.txt)
include(CMakeLists.unit_test_ucscreader.app.txt)
include(CMakeLists.unit_test_gtfreader.app.txt)
endif()
