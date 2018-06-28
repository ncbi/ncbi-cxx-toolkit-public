#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(test)
NCBI_add_app(dbapi_unit_test dbapi_context_test)
NCBI_add_subdirectory(driver)

if(OFF)

# Include projects from this directory
include(CMakeLists.dbapi_unit_test.app.txt)
include(CMakeLists.dbapi_context_test.app.txt)

# Recurse subdirectories
add_subdirectory(driver)
endif()
