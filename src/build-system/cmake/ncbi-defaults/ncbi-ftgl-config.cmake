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

get_filename_component(FTGL_CMAKE_DIR "$ENV{NCBI}/ftgl-2.1.3-rc5" REALPATH)
string(REGEX REPLACE ".*-([0-9].*)" "\\1" FTGL_VERSION_STRING "${FTGL_CMAKE_DIR}")

set(FTGL_FOUND True)
set(FTGL_INCLUDE_DIR
    ${FTGL_CMAKE_DIR}/include
    )

set(FTGL_LIBRARIES
    ${FTGL_CMAKE_DIR}/${CMAKE_BUILD_TYPE}64/lib/libftgl${_NCBI_LIBRARY_SUFFIX}
    )

get_filename_component(LIBXML2_VERSION_STRING
    "${LibXml2_CMAKE_DIR}" NAME)
string(SUBSTRING "${LIBXML2_VERSION_STRING}" 5 20 LIBXML2_VERSION_STRING)

#############################################################################
##
## Logging
##

if (_NCBI_MODULE_DEBUG)
    message(STATUS "FTGL (NCBI): FOUND = ${FTGL_FOUND}")
    message(STATUS "FTGL (NCBI): INCLUDE = ${FTGL_INCLUDE_DIR}")
    message(STATUS "FTGL (NCBI): LIBRARIES = ${FTGL_LIBRARIES}")
endif()

