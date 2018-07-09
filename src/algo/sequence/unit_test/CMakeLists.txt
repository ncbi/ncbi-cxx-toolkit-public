#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(test)
NCBI_add_app(
  unit_test_gene_model unit_test_polya unit_test_gap_analysis
  unit_test_internal_stops unit_test_align_cleanup unit_test_orf
)


if(OFF)
# Include projects from this directory
include(CMakeLists.unit_test_gene_model.app.txt)
include(CMakeLists.unit_test_polya.app.txt)
include(CMakeLists.unit_test_gap_analysis.app.txt)
include(CMakeLists.unit_test_internal_stops.app.txt)
include(CMakeLists.unit_test_align_cleanup.app.txt)
include(CMakeLists.unit_test_orf.app.txt)
endif()
