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


if (CMAKE_BUILD_TYPE STREQUAL "Debug")

    set(OPENGL_glu_LIBRARIES
        ${OpenGL_CMAKE_DIR}/lib/libGLU${_NCBI_LIBRARY_SUFFIX}
        )
    set(OPENGL_gl_LIBRARIES
        ${OpenGL_CMAKE_DIR}/lib/libGL${_NCBI_LIBRARY_SUFFIX}
        )

else()

    set(OPENGL_glu_LIBRARIES
        /opt/ncbi/64/${_MESAGL_VERSION}/lib/libGLU${_NCBI_LIBRARY_SUFFIX}
        )
    set(OPENGL_gl_LIBRARIES
        /opt/ncbi/64/${_MESAGL_VERSION}/lib/libGL${_NCBI_LIBRARY_SUFFIX}
        )


endif()

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

