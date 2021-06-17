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

set(_FTGL_VERSION "ftgl-2.1.3-rc5")

get_filename_component(FTGL_CMAKE_DIR "$ENV{NCBI}/${_FTGL_VERSION}" REALPATH)
string(REGEX REPLACE ".*-([0-9].*)" "\\1" FTGL_VERSION_STRING "${FTGL_CMAKE_DIR}")

set(FTGL_FOUND True)
set(FTGL_INCLUDE_DIR
    ${FTGL_CMAKE_DIR}/include
    )

# Choose the proper library path
# For some libraries, we look in /opt/ncbi/64
set(_libpath ${FTGL_CMAKE_DIR}/${CMAKE_BUILD_TYPE}64/lib)
if (CMAKE_BUILD_TYPE STREQUAL "Release" AND BUILD_SHARED_LIBS)
    if (EXISTS /opt/ncbi/64/${_FTGL_VERSION}/${CMAKE_BUILD_TYPE}64/lib)
        set(_libpath /opt/ncbi/64/${_FTGL_VERSION}/${CMAKE_BUILD_TYPE}64/lib)
    endif()
endif()

set(FTGL_LIBRARIES ${_libpath}/libftgl${_NCBI_LIBRARY_SUFFIX})


#############################################################################
##
## Logging
##

if (_NCBI_MODULE_DEBUG)
    message(STATUS "FTGL (NCBI): FOUND = ${FTGL_FOUND}")
    message(STATUS "FTGL (NCBI): INCLUDE = ${FTGL_INCLUDE_DIR}")
    message(STATUS "FTGL (NCBI): LIBRARIES = ${FTGL_LIBRARIES}")
endif()

