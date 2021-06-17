include(CheckFunctionExists)
include(CheckIncludeFile)

set(CMAKE_FIND_FRAMEWORK "FIRST")

find_package(BerkeleyDB)
if (BERKELEYDB_FOUND)
    set(BERKELEYDB_INCLUDE ${BerkeleyDB_INCLUDE_DIR})
    set(BERKELEYDB_LIBS    ${BerkeleyDB_LIBRARIES})

    set(HAVE_BDB 1)
    set(HAVE_BERKELEY_DB 1)
    set(HAVE_BERKELEY_DB_CXX 1)

    #
    # As a blanket statement, we now include BerkeleyDB everywhere
    # This avoids a serious insidious version skew if we have both the
    # system-installed BerkeleyDB libraries and a custom version of BerkeleyDB
    include_directories(SYSTEM ${BERKELEYDB_INCLUDE})

endif (BERKELEYDB_FOUND)


