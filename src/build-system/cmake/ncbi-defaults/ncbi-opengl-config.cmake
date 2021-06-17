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

get_filename_component(OpenGL_CMAKE_DIR "$ENV{NCBI}/${_MESAGL_VERSION}" REALPATH)
string(REGEX REPLACE ".*-([0-9].*)" "\\1" OpenGL_VERSION_STRING "${OpenGL_CMAKE_DIR}")

set(OPENGL_FOUND True)
set(OPENGL_GLU_FOUND True)
set(OPENGL_INCLUDE_DIR
    ${OpenGL_CMAKE_DIR}/include
    )

# Choose the proper library path
# For some libraries, we look in /opt/ncbi/64
set(_libpath ${OpenGL_CMAKE_DIR}/lib)
if (CMAKE_BUILD_TYPE STREQUAL "Release" AND BUILD_SHARED_LIBS)
    if (EXISTS /opt/ncbi/64/${_MESAGL_VERSION}/lib)
        set(_libpath /opt/ncbi/64/${_MESAGL_VERSION}/lib)
    endif()
endif()


set(OPENGL_glu_LIBRARIES ${_libpath}/libGLU${_NCBI_LIBRARY_SUFFIX})
set(OPENGL_gl_LIBRARIES ${_libpath}/libGL${_NCBI_LIBRARY_SUFFIX})
set(OPENGL_LIBRARIES
    ${OPENGL_glu_LIBRARIES}
    ${OPENGL_gl_LIBRARIES}
    -lXmu -lXt -lXext -lSM -lICE -lX11
    )


#############################################################################
##
## Logging
##

set(OpenGL_LIBRARIES ${OPENGL_LIBRARIES})

if (_NCBI_MODULE_DEBUG)
    message(STATUS "OpenGL (NCBI): FOUND = ${OPENGL_FOUND}")
    message(STATUS "OpenGL (NCBI): INCLUDE = ${OPENGL_INCLUDE_DIR}")
    message(STATUS "OpenGL (NCBI): LIBRARIES = ${OPENGL_LIBRARIES}")
endif()

