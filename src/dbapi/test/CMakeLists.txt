##############################################################################
# 
#

# Include projects from this directory
include(CMakeLists.dbapi_unit_test.app.txt)
include(CMakeLists.dbapi_context_test.app.txt)

# Recurse subdirectories
add_subdirectory(driver)
