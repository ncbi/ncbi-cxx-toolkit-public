#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(test)
NCBI_add_app(
  unit_test_gtfwriter
  unit_test_gff3writer
  unit_test_gff3flybasewriter
  unit_test_vcfwriter
  unit_test_wigwriter
  unit_test_bedwriter
  unit_test_bedgraphwriter
  unit_test_ucscwriter
  unit_test_srcwriter
  unit_test_alnwriter
)

if(OFF)
# Include projects from this directory
include(CMakeLists.unit_test_gtfwriter.app.txt)
include(CMakeLists.unit_test_gff3writer.app.txt)
include(CMakeLists.unit_test_vcfwriter.app.txt)
include(CMakeLists.unit_test_wigwriter.app.txt)
include(CMakeLists.unit_test_bedwriter.app.txt)
include(CMakeLists.unit_test_ucscwriter.app.txt)
include(CMakeLists.unit_test_srcwriter.app.txt)
endif()
