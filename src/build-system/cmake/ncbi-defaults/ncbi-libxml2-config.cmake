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

get_filename_component(LibXml2_CMAKE_DIR "$ENV{NCBI}/libxml-2.7.8" REALPATH)
string(REGEX REPLACE ".*-([0-9].*)" "\\1" LIBXML2_VERSION_STRING "${LibXml2_CMAKE_DIR}")

set(LIBXML2_FOUND True)
set(LIBXML2_INCLUDE_DIR
    ${LibXml2_CMAKE_DIR}/include/libxml2
    )

set(LIBXML2_LIBRARIES
    ${LibXml2_CMAKE_DIR}/${CMAKE_BUILD_TYPE}64MT/lib/libxml2${_NCBI_LIBRARY_SUFFIX}
    )
set(LIBXML2_DEFINITIONS
    )
set(LIBXML2_XMLLINT_EXECUTABLE
    ${LibXml2_CMAKE_DIR}/${CMAKE_BUILD_TYPE}64MT/bin/xmllint
    )

#############################################################################
##
## Logging
##

set(LibXml2_VERSION_STRING ${LIBXML2_VERSION_STRING})
set(LibXml2_LIBRARIES ${LIBXML2_LIBRARIES})

if (_NCBI_MODULE_DEBUG)
    message(STATUS "LibXml2 (NCBI): FOUND = ${LIBXML2_FOUND}")
    message(STATUS "LibXml2 (NCBI): INCLUDE = ${LIBXML2_INCLUDE_DIR}")
    message(STATUS "LibXml2 (NCBI): LIBRARIES = ${LIBXML2_LIBRARIES}")
    message(STATUS "LibXml2 (NCBI): DEFINITIONS = ${LIBXML2_DEFINITIONS}")
    message(STATUS "LibXml2 (NCBI): XMLLINT = ${LIBXML2_XMLLINT_EXECUTABLE}")
    message(STATUS "LibXml2 (NCBI): VERSION = ${LIBXML2_VERSION_STRING}")
endif()

