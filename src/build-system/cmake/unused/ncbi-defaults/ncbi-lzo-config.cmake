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

set(_LZO_VERSION "lzo-2.05")

get_filename_component(LZO_CMAKE_DIR "$ENV{NCBI}/${_LZO_VERSION}" REALPATH)
string(REGEX REPLACE ".*-([0-9].*)" "\\1" LZO_VERSION_STRING "${LZO_CMAKE_DIR}")

set(LZO_FOUND True)
set(LZO_INCLUDE_DIR
    ${LZO_CMAKE_DIR}/include
    )

# Choose the proper library path
# For some libraries, we look in /opt/ncbi/64
set(_libpath ${LZO_CMAKE_DIR}/lib)
if (CMAKE_BUILD_TYPE STREQUAL "Release" AND BUILD_SHARED_LIBS)
    if (EXISTS /opt/ncbi/64/${_LZO_VERSION}/lib)
        set(_libpath /opt/ncbi/64/${_LZO_VERSION}/lib)
    endif()
endif()

set(LZO_LIBRARIES ${_libpath}/liblzo2${_NCBI_LIBRARY_SUFFIX})


#############################################################################
##
## Logging
##

if (_NCBI_MODULE_DEBUG)
    message(STATUS "LZO (NCBI): FOUND = ${LZO_FOUND}")
    message(STATUS "LZO (NCBI): INCLUDE = ${LZO_INCLUDE_DIR}")
    message(STATUS "LZO (NCBI): LIBRARIES = ${LZO_LIBRARIES}")
endif()

