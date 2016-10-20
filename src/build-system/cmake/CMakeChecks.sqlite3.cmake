include(CheckFunctionExists)
include(CheckIncludeFile)

# SQLite 3.x
## set(SQLITE3_INCLUDE     ${NCBI_TOOLS_ROOT}/sqlite-3.7.13-ncbi1/include)
## set(SQLITE3_LIBS        -L${NCBI_TOOLS_ROOT}/sqlite-3.7.13-ncbi1/GCC-DebugMT64/lib -Wl,-rpath,/opt/ncbi/64/sqlite-3.7.13-ncbi1/GCC-DebugMT64/lib:${NCBI_TOOLS_ROOT}/sqlite-3.7.13-ncbi1/GCC-DebugMT64/lib -lsqlite3 )
## set(SQLITE3_STATIC_LIBS -L${NCBI_TOOLS_ROOT}/sqlite-3.7.13-ncbi1/GCC-DebugMT64/lib -lsqlite3-static)
## #set(SQLITE3_INCLUDE    ${NCBI_TOOLS_ROOT}/sqlite3/include)
## #set(SQLITE3_LIBS       -L"${NCBI_TOOLS_ROOT}/sqlite3/lib" -lsqlite3)

#
# SQLITE3
#set(CMAKE_PREFIX_PATH "$ENV{NCBI}/sqlite3/include")
#set(CMAKE_LIBRARY_PATH "$ENV{NCBI}/sqlite3/${CMAKE_BUILD_TYPE}MT64/lib")

set(CMAKE_PREFIX_PATH "${top_src_dir}/src/build-system/cmake")
#set(Sqlite3_DIR "${NCBI_TOOLS_ROOT}/sqlite-3.7.13-ncbi1")
message("SQLITE3_INCLUDE_DIR ${SQLITE3_INCLUDE_DIR}")
set(Sqlite3_DIR ".")
message("SQLITE3_INCLUDE_DIR ${SQLITE3_INCLUDE_DIR}")

#include(FindSqlite3.cmake)
set(CMAKE_FIND_FRAMEWORK "FIRST")
find_package(Sqlite3)

if (SQLITE3_FOUND)
    check_function_exists(sqlite3_unlock_notify HAVE_SQLITE3_UNLOCK_NOTIFY)
    check_include_file(sqlite3async.h HAVE_SQLITE3ASYNC_H -I${SQLITE3_INCLUDE_DIR})
endif(SQLITE3_FOUND)

set(SQLITE3_INCLUDE ${SQLITE3_INCLUDE_DIR})
set(SQLITE3_LIBS ${SQLITE3_LIBRARY})
set(SQLITE3_STATIC_LIBS ${SQLITE3_LIBS})


