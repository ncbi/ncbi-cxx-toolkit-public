#############################################################################
# $Id$
#############################################################################

NCBI_add_include_directories(${FTDS95_INCLUDE})
NCBI_add_subdirectory(freetds ctlib)

if(OFF)
# 
#

add_subdirectory(freetds)

if (SYBASE_FOUND)
  add_subdirectory(ctlib)
endif (SYBASE_FOUND)
endif()
