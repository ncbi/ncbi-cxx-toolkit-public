#############################################################################
# $Id$
#############################################################################


NCBI_add_library(dbapi_driver)
NCBI_add_subdirectory(
  util ctlib ftds64 ftds95 ftds100 ftds-default mysql odbc samples
)

if(OFF)
# 
#
#link_directories( path/to/libs ) 

# Include projects from this directory
include(CMakeLists.dbapi_driver.lib.txt)

# Recurse subdirectories
add_subdirectory(util)
add_subdirectory(ctlib)
add_subdirectory(ftds-default)
add_subdirectory(ftds64)
add_subdirectory(ftds95)
add_subdirectory(ftds100)

if (MySQL_FOUND)
    add_subdirectory(mysql)
endif()

if (${ODBC_FOUND})
add_subdirectory(odbc)
endif (${ODBC_FOUND})
add_subdirectory(samples)
endif()
