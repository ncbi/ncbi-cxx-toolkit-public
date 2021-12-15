# Config file for NCBI-specific libxml layout
#

#############################################################################
##
## Standard boilerplate capturing NCBI library layout issues
##

if (BUILD_SHARED_LIBS)
    set(_NCBI_LIBRARY_SUFFIX .a)
else()
    set(_NCBI_LIBRARY_SUFFIX -static.a)
endif()


#############################################################################
##
## Module-specific checks
##

set(_GNUTLS_VERSION "gnutls-3.4.0")

get_filename_component(GnuTLS_CMAKE_DIR "$ENV{NCBI}/${_GNUTLS_VERSION}" REALPATH)
string(REGEX REPLACE ".*-([0-9].*)" "\\1" GnuTLS_VERSION_STRING "${GnuTLS_CMAKE_DIR}")

set(GNUTLS_FOUND True)
set(GNUTLS_INCLUDE_DIR
    ${GnuTLS_CMAKE_DIR}/include
    )

# Choose the proper library path
# For some libraries, we look in /opt/ncbi/64
set(_libpath ${GnuTLS_CMAKE_DIR}/${CMAKE_BUILD_TYPE}MT64/lib)
if (CMAKE_BUILD_TYPE STREQUAL "Release" AND BUILD_SHARED_LIBS)
    if (EXISTS /opt/ncbi/64/${_GNUTLS_VERSION}/${CMAKE_BUILD_TYPE}MT64/lib)
        set(_libpath /opt/ncbi/64/${_GNUTLS_VERSION}/${CMAKE_BUILD_TYPE}MT64/lib)
    endif()
endif()

set(GNUTLS_LIBRARIES ${_libpath}/libgnutls${_NCBI_LIBRARY_SUFFIX})


#############################################################################
##
## Logging
##

set(GnuTLS_LIBRARIES ${GNUTLS_LIBRARIES})

if (_NCBI_MODULE_DEBUG)
    message(STATUS "GnuTLS (NCBI): FOUND = ${GNUTLS_FOUND}")
    message(STATUS "GnuTLS (NCBI): DIRECTORY = ${GnuTLS_CMAKE_DIR}")
    message(STATUS "GnuTLS (NCBI): VERSION = ${GnuTLS_VERSION_STRING}")
    message(STATUS "GnuTLS (NCBI): INCLUDE = ${GNUTLS_INCLUDE_DIR}")
    message(STATUS "GnuTLS (NCBI): LIBRARIES = ${GNUTLS_LIBRARIES}")
endif()

