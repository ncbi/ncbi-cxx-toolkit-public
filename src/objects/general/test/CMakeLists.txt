#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(test)
NCBI_add_app(test_date test_uoconv test_userfield)

if(OFF)
# Include projects from this directory
include(CMakeLists.test_date.app.txt)
include(CMakeLists.test_uoconv.app.txt)

if (HAVE_NCBI_C)
include(CMakeLists.test_userfield.app.txt)
endif()

endif()
