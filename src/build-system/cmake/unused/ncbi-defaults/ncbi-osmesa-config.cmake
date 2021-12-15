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

set(_MESAGL_VERSION "Mesa-7.0.2-ncbi2")

get_filename_component(OSMesa_CMAKE_DIR "$ENV{NCBI}/${_MESAGL_VERSION}" REALPATH)
string(REGEX REPLACE ".*-([0-9].*)" "\\1" OSMesa_VERSION_STRING "${OSMesa_CMAKE_DIR}")

set(OSMesa_FOUND True)
set(OSMesa_INCLUDE_DIR
    ${OSMesa_CMAKE_DIR}/include
    )

# Choose the proper library path
# For some libraries, we look in /opt/ncbi/64
set(_libpath ${OSMesa_CMAKE_DIR}/lib)
if (CMAKE_BUILD_TYPE STREQUAL "Release" AND BUILD_SHARED_LIBS)
    if (EXISTS /opt/ncbi/64/${_MESAGL_VERSION}/lib)
        set(_libpath /opt/ncbi/64/${_MESAGL_VERSION}/lib)
    endif()
endif()

set(OSMesa_LIBRARIES ${_libpath}/libOSMesa${_NCBI_LIBRARY_SUFFIX})


#############################################################################
##
## Logging
##

if (_NCBI_MODULE_DEBUG)
    message(STATUS "OSMesa (NCBI): FOUND = ${OSMesa_FOUND}")
    message(STATUS "OSMesa (NCBI): INCLUDE = ${OSMesa_INCLUDE_DIR}")
    message(STATUS "OSMesa (NCBI): LIBRARIES = ${OSMesa_LIBRARIES}")
endif()

