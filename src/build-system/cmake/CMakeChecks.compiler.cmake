#
# This config is designed to capture all compiler and linker definitions and search parameters
#

#
# See:
# http://stackoverflow.com/questions/32752446/using-compiler-prefix-commands-with-cmake-distcc-ccache
#
# There are better ways to do this
option(CMAKE_USE_CCACHE "Use 'ccache' as a preprocessor" OFF)
option(CMAKE_USE_DISTCC "Use 'distcc' as a preprocessor" OFF)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    add_definitions(-D_DEBUG -gdwarf-3)
ELSE()
    add_definitions(-DNDEBUG)
ENDIF()

if (APPLE)
  add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64)
else (APPLE)
  add_definitions(-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64)
endif (APPLE)

if (CMAKE_COMPILER_IS_GNUCC)
    add_definitions(-Wall -Wno-format-y2k )
endif()

include(CheckCXXCompilerFlag)
include(CheckCCompilerFlag)

macro(set_cxx_compiler_flag_optional)
    foreach (var ${ARGN})
        CHECK_CXX_COMPILER_FLAG("${var}" _COMPILER_SUPPORTS_FLAG)
        if (_COMPILER_SUPPORTS_FLAG)
            message(STATUS "The compiler ${CMAKE_CXX_COMPILER} supports ${var}")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${var}")
            break()
        endif()
    endforeach()
    if (_COMPILER_SUPPORTS_FLAG)
    else()
        message(WARNING "The compiler ${CMAKE_CXX_COMPILER} has no support for any of ${ARGN}")
    endif()
endmacro()

macro(set_c_compiler_flag_optional)
    foreach (var ${ARGN})
        CHECK_C_COMPILER_FLAG(${var} _COMPILER_SUPPORTS_FLAG)
        if (_COMPILER_SUPPORTS_FLAG)
            message(STATUS "The compiler ${CMAKE_C_COMPILER} supports ${var}")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${var}")
            break()
        endif()
    endforeach()
    if (_COMPILER_SUPPORTS_FLAG)
    else()
        message(WARNING "The compiler ${CMAKE_C_COMPILER} has no support for any of ${ARGN}")
    endif()
endmacro()

# Check for appropriate C++11 flags
set_cxx_compiler_flag_optional("-std=gnu++11" "-std=c++11" "-std=c++0x")
set_c_compiler_flag_optional  ("-std=gnu11" "-std=c11" "-std=gnu99" "-std=c99")


find_program(CCACHE_EXECUTABLE ccache
             PATHS /usr/local/ccache/3.2.5/bin/)
find_program(DISTCC_EXECUTABLE distcc)

if (CMAKE_USE_DISTCC AND DISTCC_EXECUTABLE)

    message(STATUS "Enabling distcc: ${DISTCC_EXECUTABLE}")
    set(CMAKE_C_COMPILER_ARG1 ${CMAKE_C_COMPILER})
    set(CMAKE_CXX_COMPILER_ARG1 ${CMAKE_CXX_COMPILER})

    set(CMAKE_C_COMPILER ${DISTCC_EXECUTABLE})
    set(CMAKE_CXX_COMPILER ${DISTCC_EXECUTABLE})

elseif(CMAKE_USE_CCACHE AND CCACHE_EXECUTABLE)

    set(CMAKE_C_COMPILER_ARG1 ${CMAKE_C_COMPILER})
    set(CMAKE_CXX_COMPILER_ARG1 ${CMAKE_CXX_COMPILER})

    set(CMAKE_C_COMPILER ${CCACHE_EXECUTABLE})
    set(CMAKE_CXX_COMPILER ${CCACHE_EXECUTABLE})

    set(CMAKE_C_COMPILE_OBJECT "CCACHE_BASEDIR=${top_src_dir} ${CMAKE_C_COMPILE_OBJECT}")
    set(CMAKE_CXX_COMPILE_OBJECT "CCACHE_BASEDIR=${top_src_dir} ${CMAKE_CXX_COMPILE_OBJECT}")

    # pass these back for ccache to pick up
    set(ENV{CCACHE_BASEDIR} ${top_src_dir})
    message(STATUS "Enabling ccache: ${CCACHE_EXECUTABLE}")
    message(STATUS "  ccache basedir: ${top_src_dir}")

endif()




#
# NOTE:
# uncomment this for strict mode for library compilation
#
#set(CMAKE_SHARED_LINKER_FLAGS_DYNAMIC "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--export-dynamic")

set(CMAKE_SHARED_LINKER_FLAGS_RDYNAMIC "${CMAKE_SHARED_LINKER_FLAGS}") # for smooth transition, please don't use
set(CMAKE_SHARED_LINKER_FLAGS_ALLOW_UNDEFINED "${CMAKE_SHARED_LINKER_FLAGS}")
if ((NOT DEFINED ${APPLE}) OR (NOT ${APPLE}))
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
endif ()
