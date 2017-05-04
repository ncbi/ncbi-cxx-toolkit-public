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

get_filename_component(GLEW_CMAKE_DIR "$ENV{NCBI}/glew" REALPATH)
string(REGEX REPLACE ".*-([0-9].*)" "\\1" GLEW_VERSION_STRING "${GLEW_CMAKE_DIR}")

set(GLEW_FOUND True)
set(GLEW_INCLUDE_DIRS
    ${GLEW_CMAKE_DIR}/include
    )

set(GLEW_LIBRARIES
    ${GLEW_CMAKE_DIR}/${CMAKE_BUILD_TYPE}64MT/lib/libGLEW${_NCBI_LIBRARY_SUFFIX}
    )


#############################################################################
##
## Logging
##

if (_NCBI_MODULE_DEBUG)
    message(STATUS "GLEW (NCBI): FOUND = ${GLEW_FOUND}")
    message(STATUS "GLEW (NCBI): INCLUDE = ${GLEW_INCLUDE_DIRS}")
    message(STATUS "GLEW (NCBI): LIBRARIES = ${GLEW_LIBRARIES}")
endif()

