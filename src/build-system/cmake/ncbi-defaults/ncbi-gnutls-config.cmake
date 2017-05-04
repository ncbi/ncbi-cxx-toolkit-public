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

get_filename_component(GnuTLS_CMAKE_DIR "$ENV{NCBI}/gnutls-3.4.0" REALPATH)
string(REGEX REPLACE ".*-([0-9].*)" "\\1" GnuTLS_VERSION_STRING "${GnuTLS_CMAKE_DIR}")

set(GNUTLS_FOUND True)
set(GNUTLS_INCLUDE_DIR
    ${GnuTLS_CMAKE_DIR}/include
    )

set(GNUTLS_LIBRARIES
    ${GnuTLS_CMAKE_DIR}/${CMAKE_BUILD_TYPE}64MT/lib/libgnutls${_NCBI_LIBRARY_SUFFIX}
    )


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

