##############################################################################
# 
#

add_subdirectory(freetds)

if (SYBASE_FOUND)
  add_subdirectory(ctlib)
endif (SYBASE_FOUND)
