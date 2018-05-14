#############################################################################
# $Id$
#############################################################################
#
# This config is designed to capture all compiler and linker definitions and search parameters
#

if (NCBI_EXPERIMENTAL_CFG)

  set(CMAKE_CXX_STANDARD 11)
 
#----------------------------------------------------------------------------
if (WIN32)

  set(CMAKE_CONFIGURATION_TYPES DebugDLL ReleaseDLL DebugMT ReleaseMT)
  set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "Reset the configurations" FORCE)

  set(CMAKE_CXX_FLAGS_DEBUGDLL   "/MDd /Zi /Od /RTC1 /D_DEBUG")
  set(CMAKE_CXX_FLAGS_DEBUGMT    "/MTd /Zi /Od /RTC1 /D_DEBUG")
  set(CMAKE_CXX_FLAGS_RELEASEDLL "/MD  /Zi /O2 /Ob1 /DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELEASEMT  "/MT  /Zi /O2 /Ob1 /DNDEBUG")

  set(CMAKE_C_FLAGS_DEBUGDLL   "/MDd /Zi /Od /RTC1 /D_DEBUG")
  set(CMAKE_C_FLAGS_DEBUGMT    "/MTd /Zi /Od /RTC1 /D_DEBUG")
  set(CMAKE_C_FLAGS_RELEASEDLL "/MD  /Zi /O2 /Ob1 /DNDEBUG")
  set(CMAKE_C_FLAGS_RELEASEMT  "/MT  /Zi /O2 /Ob1 /DNDEBUG")

  set(CMAKE_EXE_LINKER_FLAGS_DEBUGDLL   "/DEBUG /INCREMENTAL:NO")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUGMT    "/DEBUG /INCREMENTAL:NO")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASEDLL "/INCREMENTAL:NO")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASEMT  "/INCREMENTAL:NO")

  set(CMAKE_SHARED_LINKER_FLAGS_DEBUGDLL   "/DEBUG /INCREMENTAL:NO")
  set(CMAKE_SHARED_LINKER_FLAGS_DEBUGMT    "/DEBUG /INCREMENTAL:NO")
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASEDLL "/INCREMENTAL:NO")
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASEMT  "/INCREMENTAL:NO")

  add_definitions(-D_CRT_SECURE_NO_WARNINGS=1)
  if(NCBI_EXPERIMENTAL_DLL)
    add_definitions(-DNCBI_DLL_BUILD)
  endif()

  set(NCBI_DEFAULT_USEPCH ON)
  set(NCBI_DEFAULT_PCH "ncbi_pch.hpp")
  set(NCBI_DEFAULT_PCH_DEFINE "NCBI_USE_PCH")
  set(NCBI_DEFAULT_RESOURCES "${NCBI_SRC_ROOT}//build-system/cmake/ncbi.rc")

  set(ORIG_LIBS dbghelp.lib)
  return()

#----------------------------------------------------------------------------
elseif (XCODE)

  set(CMAKE_CONFIGURATION_TYPES DebugDLL ReleaseDLL DebugMT ReleaseMT)
  set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "Reset the configurations" FORCE)

  set(CMAKE_CXX_FLAGS_DEBUGDLL   "-gdwarf-4 -ggdb3 -O0 -D_DEBUG")
  set(CMAKE_CXX_FLAGS_DEBUGMT    "-gdwarf-4 -ggdb3 -O0 -D_DEBUG")
  set(CMAKE_CXX_FLAGS_RELEASEDLL "-gdwarf-4 -ggdb1 -O3 -DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELEASEMT  "-gdwarf-4 -ggdb1 -O3 -DNDEBUG")

  set(CMAKE_C_FLAGS_DEBUGDLL   "-gdwarf-4 -ggdb3 -g -O0 -D_DEBUG")
  set(CMAKE_C_FLAGS_DEBUGMT    "-gdwarf-4 -ggdb3 -g -O0 -D_DEBUG")
  set(CMAKE_C_FLAGS_RELEASEDLL "-gdwarf-4 -ggdb1 -O3 -DNDEBUG")
  set(CMAKE_C_FLAGS_RELEASEMT  "-gdwarf-4 -ggdb1 -O3 -DNDEBUG")

  set(CMAKE_EXE_LINKER_FLAGS_DEBUGDLL   "-stdlib=libc++ -framework CoreServices")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUGMT    "-stdlib=libc++ -framework CoreServices")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASEDLL "-stdlib=libc++ -framework CoreServices")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASEMT  "-stdlib=libc++ -framework CoreServices")

  add_definitions(-DNCBI_XCODE_BUILD)

  find_package(Threads REQUIRED)
  if (CMAKE_USE_PTHREADS_INIT)
    add_definitions(-D_MT -D_REENTRANT -D_THREAD_SAFE)
    set(NCBI_POSIX_THREADS 1)
  endif (CMAKE_USE_PTHREADS_INIT)
  return()

#----------------------------------------------------------------------------
else()
endif()

endif()


#
# Threading libraries
find_package(Threads REQUIRED)
## message("CMAKE_THREAD_LIBS_INIT: ${CMAKE_THREAD_LIBS_INIT}")
## message("CMAKE_USE_SPROC_INIT: ${CMAKE_USE_SPROC_INIT}")
## message("CMAKE_USE_WIN32_THREADS_INIT: ${CMAKE_USE_WIN32_THREADS_INIT}")
## message("CMAKE_USE_PTHREADS_INIT: ${CMAKE_USE_PTHREADS_INIT}")
## message("CMAKE_HP_PTHREADS_INIT: ${CMAKE_HP_PTHREADS_INIT}")
if (CMAKE_USE_PTHREADS_INIT)
    add_definitions(-D_MT -D_REENTRANT -D_THREAD_SAFE)
    set(NCBI_POSIX_THREADS 1)
endif (CMAKE_USE_PTHREADS_INIT)


#
# See:
# http://stackoverflow.com/questions/32752446/using-compiler-prefix-commands-with-cmake-distcc-ccache
#
# There are better ways to do this
option(CMAKE_USE_CCACHE "Use 'ccache' as a preprocessor" OFF)
option(CMAKE_USE_DISTCC "Use 'distcc' as a preprocessor" OFF)

if (NOT WIN32)
    if ("${CMAKE_DEBUG_SYMBOLS}" STREQUAL "")
        set(CMAKE_CXX_FLAGS_RELEASE "-gdwarf-4 -ggdb1 -O3 -DNDEBUG" CACHE STRING "" FORCE)
        set(CMAKE_C_FLAGS_RELEASE "-gdwarf-4 -ggdb1 -O3 -DNDEBUG" CACHE STRING "" FORCE)
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "-gdwarf-4 -ggdb3 -O3 -DNDEBUG" CACHE STRING "" FORCE)
        set(CMAKE_C_FLAGS_RELEASE "-gdwarf-4 -ggdb3 -O3 -DNDEBUG" CACHE STRING "" FORCE)
    endif()
    set(CMAKE_CXX_FLAGS_DEBUG "-gdwarf-4 -ggdb3 -O0 -D_DEBUG" CACHE STRING "" FORCE)
    set(CMAKE_C_FLAGS_DEBUG "-gdwarf-4 -ggdb3 -O0 -D_DEBUG" CACHE STRING "" FORCE)
endif() 

if (APPLE)
  add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64)
elseif (UNIX)
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
if (UNIX)
	set_cxx_compiler_flag_optional("-std=gnu++11" "-std=c++11" "-std=c++0x")
	set_c_compiler_flag_optional  ("-std=gnu11" "-std=c11" "-std=gnu99" "-std=c99")
endif()

find_program(CCACHE_EXECUTABLE ccache
             PATHS /usr/local/ccache/3.2.5/bin/)
find_program(DISTCC_EXECUTABLE distcc)


set(NCBI_COMPILER_WRAPPER "")
if (CMAKE_USE_DISTCC AND DISTCC_EXECUTABLE AND
    CMAKE_USE_CCACHE AND CCACHE_EXECUTABLE)
set(NCBI_COMPILER_WRAPPER "CCACHE_BASEDIR=${top_src_dir} CCACHE_PREFIX=${DISTCC_EXECUTABLE} ${CCACHE_EXECUTABLE} ${NCBI_COMPILER_WRAPPER}")
elseif (CMAKE_USE_DISTCC AND DISTCC_EXECUTABLE)
    set(NCBI_COMPILER_WRAPPER "${DISTCC_EXECUTABLE} ${NCBI_COMPILER_WRAPPER}")
elseif(CMAKE_USE_CCACHE AND CCACHE_EXECUTABLE)
    set(NCBI_COMPILER_WRAPPER "CCACHE_BASEDIR=${top_src_dir} ${CCACHE_EXECUTABLE} ${NCBI_COMPILER_WRAPPER}")
    # pass these back for ccache to pick up
    set(ENV{CCACHE_BASEDIR} ${top_src_dir})
    message(STATUS "Enabling ccache: ${CCACHE_EXECUTABLE}")
    message(STATUS "  ccache basedir: ${top_src_dir}")
endif()

set(CMAKE_CXX_COMPILE_OBJECT
    "${NCBI_COMPILER_WRAPPER} ${CMAKE_CXX_COMPILE_OBJECT}")

set(CMAKE_C_COMPILE_OBJECT
    "${NCBI_COMPILER_WRAPPER} ${CMAKE_C_COMPILE_OBJECT}")

message(STATUS "NCBI_COMPILER_WRAPPER = ${NCBI_COMPILER_WRAPPER}")


if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    set(NCBI_COMPILER_GCC 1)
    set(NCBI_COMPILER "GCC")
elseif(MSVC)
    set(NCBI_COMPILER "MSVC")
    set(NCBI_COMPILER_GCC 0)
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


if (BUILD_SHARED_LIBS)
    set(NCBI_DLL_BUILD 1)
    set(NCBI_DLL_SUPPORT 1)
endif()


# Establishing sane RPATH definitions
# use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, use the install RPATH already
SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

SET(CMAKE_INSTALL_RPATH "/$ORIGIN/../lib")

#this add RUNPATH to binaries (RPATH is already there anyway), which makes it more like binaries built by C++ Toolkit
if (NOT WIN32)
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--enable-new-dtags")
endif()

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH true)
