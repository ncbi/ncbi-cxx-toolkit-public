# Config file for NCBI-specific libxml layout
#

#############################################################################
##
## Standard boilerplate capturing NCBI library layout issues
##

if (BUILD_SHARED_LIBS)
    set(_NCBI_LIBRARY_SUFFIX .so)
else()
    set(_NCBI_LIBRARY_SUFFIX -static.a)
endif()


#############################################################################
##
## Module-specific checks
##

set(_BDB_VERSION "BerkeleyDB-4.6.21.1")

get_filename_component(BerkeleyDB_CMAKE_DIR "$ENV{NCBI}/${_BDB_VERSION}" REALPATH)
string(REGEX REPLACE ".*-([0-9].*)" "\\1" BerkeleyDB_VERSION_STRING "${BerkeleyDB_CMAKE_DIR}")

set(BERKELEYDB_FOUND True)
set(BERKELEYDB_INCLUDE_DIR
    ${BerkeleyDB_CMAKE_DIR}/include
    )

# Choose the proper library path
# For some libraries, we look in /opt/ncbi/64
set(_libpath ${BerkeleyDB_CMAKE_DIR}/${CMAKE_BUILD_TYPE}MT64)
if (CMAKE_BUILD_TYPE STREQUAL "Release" AND BUILD_SHARED_LIBS)
    if (EXISTS /opt/ncbi/64/${_BDB_VERSION}/${CMAKE_BUILD_TYPE}MT64)
        set(_libpath /opt/ncbi/64/${_BDB_VERSION}/${CMAKE_BUILD_TYPE}MT64)
    endif()
endif()

set(BERKELEYDB_LIBRARY ${_libpath}/libdb${_NCBI_LIBRARY_SUFFIX})
set(BERKELEYDB_LIBRARIES ${BERKELEYDB_LIBRARY})

# These are for compatibility
# Some variants of CMake are case-sensitive when it comes to variables
set(BerkeleyDB_INCLUDE_DIR ${BERKELEYDB_INCLUDE_DIR})
set(BerkeleyDB_LIBRARY ${BERKELEYDB_LIBRARY})
set(BerkeleyDB_LIBRARIES ${BERKELEYDB_LIBRARIES})
set(BerkeleyDB_FOUND ${BERKELEYDB_FOUND})

#############################################################################
##
## Logging
##

if (_NCBI_MODULE_DEBUG)
    message(STATUS "BerkeleyDB (NCBI): FOUND = ${BerkeleyDB_FOUND}")
    message(STATUS "BerkeleyDB (NCBI): VERSION = ${BerkeleyDB_VERSION_STRING}")
    message(STATUS "BerkeleyDB (NCBI): INCLUDE = ${BERKELEYDB_INCLUDE_DIR}")
    message(STATUS "BerkeleyDB (NCBI): INCLUDE = ${BERKELEYDB_INCLUDE_DIR}")
    message(STATUS "BerkeleyDB (NCBI): LIBRARIES = ${BERKELEYDB_LIBRARY}")
endif()

