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

set(_LIBXSLT_VERSION "libxml-2.7.8")

get_filename_component(LibXslt_CMAKE_DIR "$ENV{NCBI}/${_LIBXSLT_VERSION}" REALPATH)
string(REGEX REPLACE ".*-([0-9].*)" "\\1" LibXslt_VERSION_STRING "${LibXslt_CMAKE_DIR}")

set(LIBXSLT_FOUND True)
set(LIBXSLT_INCLUDE_DIR
    ${LibXslt_CMAKE_DIR}/include/libxslt
    )

set(LIBXSLT_DEFINITIONS
    )
set(LIBXSLT_XSLTPROC_EXECUTABLE
    ${LibXslt_CMAKE_DIR}/${CMAKE_BUILD_TYPE}64MT/bin/xsltproc
    )

# Choose the proper library path
# For some libraries, we look in /opt/ncbi/64
set(_libpath ${LibXslt_CMAKE_DIR}/${CMAKE_BUILD_TYPE}MT64/lib)
if (CMAKE_BUILD_TYPE STREQUAL "Release" AND BUILD_SHARED_LIBS)
    if (EXISTS /opt/ncbi/64/${_LIBXSLT_VERSION}/${CMAKE_BUILD_TYPE}MT64/lib)
        set(_libpath /opt/ncbi/64/${_LIBXSLT_VERSION}/${CMAKE_BUILD_TYPE}MT64/lib)
    endif()
endif()

set(LIBXSLT_LIBRARIES ${_libpath}/libxslt${_NCBI_LIBRARY_SUFFIX})
set(LIBXSLT_EXSLT_LIBRARIES ${_libpath}/libexslt${_NCBI_LIBRARY_SUFFIX})


#############################################################################
##
## Logging
##

set(LIBXSLT_VERSION_STRING ${LibXslt_VERSION_STRING})
set(LibXslt_LIBRARIES ${LIBXSLT_LIBRARIES})

if (_NCBI_MODULE_DEBUG)
    message(STATUS "LibXslt (NCBI): FOUND = ${LIBXSLT_FOUND}")
    message(STATUS "LibXslt (NCBI): INCLUDE = ${LIBXSLT_INCLUDE_DIR}")
    message(STATUS "LibXslt (NCBI): LIBRARIES = ${LIBXSLT_LIBRARIES}")
    message(STATUS "LibXslt (NCBI): DEFINITIONS = ${LIBXSLT_DEFINITIONS}")
    message(STATUS "LibXslt (NCBI): XSLTPROC = ${LIBXSLT_XSLTPROC_EXECUTABLE}")
    message(STATUS "LibXslt (NCBI): VERSION = ${LIBXSLT_VERSION_STRING}")
endif()

