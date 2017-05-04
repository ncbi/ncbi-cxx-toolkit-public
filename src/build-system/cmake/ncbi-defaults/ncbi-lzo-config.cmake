# Config file for NCBI-specific libxml layout
#

#############################################################################
##
## Standard boilerplate capturing NCBI library layout issues
##

if (BUILD_SHARED_LIBS)
    set(_NCBI_LIBRARY_SUFFIX -dll.so)
else()
    set(_NCBI_LIBRARY_SUFFIX -static.a)
endif()


#############################################################################
##
## Module-specific checks
##

get_filename_component(LZO_CMAKE_DIR "$ENV{NCBI}/lzo-2.05" REALPATH)
string(REGEX REPLACE ".*-([0-9].*)" "\\1" LZO_VERSION_STRING "${LZO_CMAKE_DIR}")

set(LZO_FOUND True)
set(LZO_INCLUDE_DIR
    ${LZO_CMAKE_DIR}/include
    )

set(LZO_LIBRARIES
    ${LZO_CMAKE_DIR}/lib/liblzo2${_NCBI_LIBRARY_SUFFIX}
    )


#############################################################################
##
## Logging
##

if (_NCBI_MODULE_DEBUG)
    message(STATUS "LZO (NCBI): FOUND = ${LZO_FOUND}")
    message(STATUS "LZO (NCBI): INCLUDE = ${LZO_INCLUDE_DIR}")
    message(STATUS "LZO (NCBI): LIBRARIES = ${LZO_LIBRARIES}")
endif()

