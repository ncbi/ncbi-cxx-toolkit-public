include(CheckFunctionExists)
include(CheckIncludeFile)

set(CMAKE_FIND_FRAMEWORK "FIRST")

set(CMAKE_PREFIX_PATH /opt/ncbi/64/BerkeleyDB-4.6.21.1/${CMAKE_BUILD_TYPE})
find_package(BerkeleyDB)
set(CMAKE_PREFIX_PATH )

if (BERKELEYDB_FOUND)
  set(HAVE_BDB 1)
  set(HAVE_BERKELEY_DB 1)
  set(HAVE_BERKELEY_DB_CXX 1)

endif (BERKELEYDB_FOUND)


