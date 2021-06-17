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

set(_LIBXML_VERSION "libxml-2.7.8")

get_filename_component(LibXml2_CMAKE_DIR "$ENV{NCBI}/${_LIBXML_VERSION}" REALPATH)
string(REGEX REPLACE ".*-([0-9].*)" "\\1" LIBXML2_VERSION_STRING "${LibXml2_CMAKE_DIR}")

set(LIBXML2_FOUND True)
set(LIBXML2_INCLUDE_DIR
    ${LibXml2_CMAKE_DIR}/include/libxml2
    )


# Choose the proper library path
# For some libraries, we look in /opt/ncbi/64
set(_libpath ${LibXml2_CMAKE_DIR}/${CMAKE_BUILD_TYPE}MT64/lib)
if (CMAKE_BUILD_TYPE STREQUAL "Release" AND BUILD_SHARED_LIBS)
    if (EXISTS /opt/ncbi/64/${_LIBXML_VERSION}/${CMAKE_BUILD_TYPE}MT64/lib)
        set(_libpath /opt/ncbi/64/${_LIBXML_VERSION}/${CMAKE_BUILD_TYPE}MT64/lib)
    endif()
endif()

set(LIBXML2_LIBRARIES ${_libpath}/libxml2${_NCBI_LIBRARY_SUFFIX})


set(LIBXML2_DEFINITIONS    )
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

