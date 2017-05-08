##############################################################################
# 
#

include_directories(SYSTEM ${BOOST_INCLUDE})

# Include projects from this directory
include(CMakeLists.unit_test_alnmgr.app.txt)
include(CMakeLists.unit_test_score_builder.app.txt)

# Recurse subdirectories
