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

get_filename_component(BerkeleyDB_CMAKE_DIR "$ENV{NCBI}/BerkeleyDB-4.6.21.1" REALPATH)
string(REGEX REPLACE ".*-([0-9].*)" "\\1" BerkeleyDB_VERSION_STRING "${BerkeleyDB_CMAKE_DIR}")

set(BerkeleyDB_FOUND True)
set(BERKELEYDB_INCLUDE_DIR
    ${BerkeleyDB_CMAKE_DIR}/include
    )

set(BERKELEYDB_LIBRARY
    ${BerkeleyDB_CMAKE_DIR}/${CMAKE_BUILD_TYPE}MT64/libdb${_NCBI_LIBRARY_SUFFIX}
    )
set(BERKELEYDB_LIBS
    -Wl,-rpath,/opt/ncbi/64/BerkeleyDB-4.6.21.1/${CMAKE_BUILD_TYPE}MT64 ${BERKELEYDB_LIBRARY}
    )


#############################################################################
##
## Logging
##

set(BerkeleyDB_LIBRARIES ${BERKELEYDB_LIBRARY})

if (_NCBI_MODULE_DEBUG)
    message(STATUS "BerkeleyDB (NCBI): FOUND = ${BerkeleyDB_FOUND}")
    message(STATUS "BerkeleyDB (NCBI): VERSION = ${BerkeleyDB_VERSION_STRING}")
    message(STATUS "BerkeleyDB (NCBI): INCLUDE = ${BERKELEYDB_INCLUDE_DIR}")
    message(STATUS "BerkeleyDB (NCBI): LIBRARIES = ${BERKELEYDB_LIBRARY}")
endif()

