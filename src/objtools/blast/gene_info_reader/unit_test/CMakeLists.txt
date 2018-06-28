#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(test)
NCBI_requires(Boost.Test.Included)
NCBI_add_app(gene_info_unit_test)

if(OFF)
# Include projects from this directory
if (UNIX AND NOT APPLE)
include(CMakeLists.gene_info_unit_test.app.txt)
endif()
endif()

