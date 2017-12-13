include(CheckFunctionExists)
include(CheckIncludeFile)

find_package(Sqlite3)

set(SQLITE3_INCLUDE ${SQLITE3_INCLUDE_DIR})
set(SQLITE3_LIBS ${SQLITE3_LIBRARY})
set(SQLITE3_STATIC_LIBS ${SQLITE3_LIBS})

if (SQLITE3_FOUND)
    set(CMAKE_REQUIRED_LIBRARIES_OLD "${CMAKE_REQUIRED_LIBRARIES}")
    set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${SQLITE3_LIBRARY} ${LIBS})

    check_symbol_exists(sqlite3_unlock_notify ${SQLITE3_INCLUDE_DIR}/sqlite3.h HAVE_SQLITE3_UNLOCK_NOTIFY)
    if (HAVE_SQLITE3_UNLOCK_NOTIFY)
        message(STATUS "  SQLite3: Found sqlite3_unlock_notify")
    endif()
    set(CMAKE_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES_OLD}")

    check_include_file(sqlite3async.h HAVE_SQLITE3ASYNC_H -I${SQLITE3_INCLUDE_DIR})
    if (HAVE_SQLITE3ASYNC_H)
        message(STATUS "  SQLite3: Found sqlite3async.h")
    endif()

    #
    # As a blanket statement, we now include SQLite3 everywhere
    # This avoids a serious insidious version skew if we have both the
    # system-installed SQLite3 libraries and a custom version of SQLite3
    include_directories(SYSTEM ${SQLITE3_INCLUDE})

endif(SQLITE3_FOUND)


