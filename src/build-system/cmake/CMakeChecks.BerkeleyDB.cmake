include(CheckFunctionExists)
include(CheckIncludeFile)

set(CMAKE_FIND_FRAMEWORK "FIRST")

find_package(BerkeleyDB)
if (BERKELEYDB_FOUND)
    set(BERKELEYDB_INCLUDE ${BerkeleyDB_INCLUDE_DIR})
    set(BERKELEYDB_LIBS    ${BerkeleyDB_LIBRARIES})
endif()

if (BERKELEYDB_FOUND)
  set(HAVE_BDB 1)
  set(HAVE_BERKELEY_DB 1)
  set(HAVE_BERKELEY_DB_CXX 1)

endif (BERKELEYDB_FOUND)


