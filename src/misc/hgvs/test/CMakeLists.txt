#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(test)
NCBI_add_app(hgvs2variation test_hgvs_parser test_hgvs_reader)
# 
#

# Include projects from this directory
#include(CMakeLists.hgvs2variation.app.txt)
#include(CMakeLists.test_hgvs_parser.app.txt)
#include(CMakeLists.test_hgvs_reader.app.txt)

