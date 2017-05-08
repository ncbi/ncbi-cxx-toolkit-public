##############################################################################
# 
#

include_directories(SYSTEM ${BOOST_INCLUDE})

# Include projects from this directory
include(CMakeLists.unit_test_gtfwriter.app.txt)
include(CMakeLists.unit_test_gff3writer.app.txt)
include(CMakeLists.unit_test_vcfwriter.app.txt)
include(CMakeLists.unit_test_wigwriter.app.txt)
include(CMakeLists.unit_test_bedwriter.app.txt)
include(CMakeLists.unit_test_ucscwriter.app.txt)
include(CMakeLists.unit_test_srcwriter.app.txt)

