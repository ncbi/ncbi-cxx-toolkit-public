include(CheckFunctionExists)
include(CheckIncludeFile)

find_package(Sqlite3)

if (SQLITE3_FOUND)
    check_function_exists(sqlite3_unlock_notify HAVE_SQLITE3_UNLOCK_NOTIFY)
    check_include_file(sqlite3async.h HAVE_SQLITE3ASYNC_H -I${SQLITE3_INCLUDE_DIR})
endif(SQLITE3_FOUND)

set(SQLITE3_INCLUDE ${SQLITE3_INCLUDE_DIR})
set(SQLITE3_LIBS ${SQLITE3_LIBRARY})
set(SQLITE3_STATIC_LIBS ${SQLITE3_LIBS})


