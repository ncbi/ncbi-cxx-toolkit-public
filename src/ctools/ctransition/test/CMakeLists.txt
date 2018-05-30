#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(test)
NCBI_add_app(test_ctransition test_ctransition_nlmzip)

if(OFF)
include(CMakeLists.test_ctransition.app.txt)
endif()
