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

get_filename_component(OSMesa_CMAKE_DIR "$ENV{NCBI}/MesaGL" REALPATH)
string(REGEX REPLACE ".*-([0-9].*)" "\\1" OSMesa_VERSION_STRING "${OSMesa_CMAKE_DIR}")

set(OSMesa_FOUND True)
set(OSMesa_INCLUDE_DIR
    ${OSMesa_CMAKE_DIR}/include
    )

set(OSMesa_LIBRARIES
    ${OSMesa_CMAKE_DIR}/lib/libOSMesa${_NCBI_LIBRARY_SUFFIX}
    )


#############################################################################
##
## Logging
##

if (_NCBI_MODULE_DEBUG)
    message(STATUS "OSMesa (NCBI): FOUND = ${OSMesa_FOUND}")
    message(STATUS "OSMesa (NCBI): INCLUDE = ${OSMesa_INCLUDE_DIR}")
    message(STATUS "OSMesa (NCBI): LIBRARIES = ${OSMesa_LIBRARIES}")
endif()

