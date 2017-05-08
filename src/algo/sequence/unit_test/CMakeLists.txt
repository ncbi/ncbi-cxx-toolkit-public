##############################################################################
# 
#

include_directories(SYSTEM ${BOOST_INCLUDE})

# Include projects from this directory
include(CMakeLists.unit_test_gene_model.app.txt)
include(CMakeLists.unit_test_polya.app.txt)
include(CMakeLists.unit_test_gap_analysis.app.txt)
include(CMakeLists.unit_test_internal_stops.app.txt)
include(CMakeLists.unit_test_align_cleanup.app.txt)
include(CMakeLists.unit_test_orf.app.txt)

