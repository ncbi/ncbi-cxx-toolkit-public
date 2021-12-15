# Config file for NCBI-specific libxml layout
#

#############################################################################
##
## Standard boilerplate capturing NCBI library layout issues
##

if (BUILD_SHARED_LIBS)
    set(_NCBI_LIBRARY_SUFFIX .a)
    set(_NCBI_LIBRARY_STATIC )
else()
    set(_NCBI_LIBRARY_SUFFIX .a)
    set(_NCBI_LIBRARY_STATIC )
endif()


#############################################################################
##
## Module-specific checks
##

set(_LEVELDB_VERSION "leveldb-1.21")

get_filename_component(LEVELDB_CMAKE_DIR "$ENV{NCBI}/${_LEVELDB_VERSION}" REALPATH)
string(REGEX REPLACE ".*-([0-9].*)" "\\1" LEVELDB_VERSION_STRING "${LEVELDB_CMAKE_DIR}")

set(LEVELDB_FOUND True)
set(LEVELDB_INCLUDE_DIR
    ${LEVELDB_CMAKE_DIR}/include
    )

# Choose the proper library path
# For some libraries, we look in /opt/ncbi/64
set(_libpath_leveldb ${LEVELDB_CMAKE_DIR})
if (CMAKE_BUILD_TYPE STREQUAL "Release" AND BUILD_SHARED_LIBS)
    if (EXISTS /opt/ncbi/64/${_LEVELDB_VERSION})
        set(_libpath_leveldb /opt/ncbi/64/${_LEVELDB_VERSION})
    endif()
endif()

if (EXISTS "${_libpath_leveldb}/${NCBI_COMPILER}${NCBI_COMPILER_VERSION}-${CMAKE_BUILD_TYPE}/lib")
    set(_libpath_leveldb "${_libpath_leveldb}/${NCBI_COMPILER}${NCBI_COMPILER_VERSION}-${CMAKE_BUILD_TYPE}/lib")
else()
    set(_libpath_leveldb "${_libpath_leveldb}/lib")
endif()

set(LEVELDB_LIBRARIES
    ${_libpath_leveldb}/libleveldb${_NCBI_LIBRARY_SUFFIX}
    )

set(LEVELDB_INCLUDE_DIRS ${LEVELDB_INCLUDE_DIR})

#############################################################################
##
## Logging
##

#if (_NCBI_MODULE_DEBUG)
if (True)
    message(STATUS "LEVELDB (NCBI): FOUND = ${LEVELDB_FOUND}")
    message(STATUS "LEVELDB (NCBI): INCLUDE = ${LEVELDB_INCLUDE_DIR}")
    message(STATUS "LEVELDB (NCBI): LIBRARIES = ${LEVELDB_LIBRARIES}")
endif()

