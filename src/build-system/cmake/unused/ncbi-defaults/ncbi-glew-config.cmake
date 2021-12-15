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

set(_GLEW_VERSION "glew-1.5.8")

get_filename_component(GLEW_CMAKE_DIR "$ENV{NCBI}/glew-1.5.8" REALPATH)
string(REGEX REPLACE ".*-([0-9].*)" "\\1" GLEW_VERSION_STRING "${GLEW_CMAKE_DIR}")

set(GLEW_FOUND True)
set(GLEW_INCLUDE_DIRS
    ${GLEW_CMAKE_DIR}/include
    )

# Choose the proper library path
# For some libraries, we look in /opt/ncbi/64
set(_libpath ${GLEW_CMAKE_DIR}/${CMAKE_BUILD_TYPE}64/lib)
if (CMAKE_BUILD_TYPE STREQUAL "Release" AND BUILD_SHARED_LIBS)
    if (EXISTS /opt/ncbi/64/${_GLEW_VERSION}/${CMAKE_BUILD_TYPE}64/lib)
        set(_libpath /opt/ncbi/64/${_GLEW_VERSION}/${CMAKE_BUILD_TYPE}64/lib)
    endif()
endif()

set(GLEW_LIBRARIES ${_libpath}/libGLEW${_NCBI_LIBRARY_SUFFIX})



#############################################################################
##
## Logging
##

if (_NCBI_MODULE_DEBUG)
    message(STATUS "GLEW (NCBI): FOUND = ${GLEW_FOUND}")
    message(STATUS "GLEW (NCBI): INCLUDE = ${GLEW_INCLUDE_DIRS}")
    message(STATUS "GLEW (NCBI): LIBRARIES = ${GLEW_LIBRARIES}")
endif()

