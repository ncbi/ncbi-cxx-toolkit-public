#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper
##    Author: Andrei Gourianov, gouriano@ncbi
##
##  Source tree description


string(TIMESTAMP NCBI_TIMESTAMP_START "%s")
string(TIMESTAMP _start)
message("Started: ${_start}")

#############################################################################
if("${CMAKE_GENERATOR}" STREQUAL "Xcode")
    if(NOT DEFINED XCODE)
        set(XCODE ON)
    endif()
endif()
if("$ENV{OSTYPE}" STREQUAL "cygwin")
    if(NOT DEFINED CYGWIN)
        set(CYGWIN ON)
    endif()
endif()

if(DEFINED NCBIPTB.env.NCBI)
    set(ENV{NCBI} ${NCBIPTB.env.NCBI})
endif()
if(DEFINED NCBIPTB.env.PATH)
    if(WIN32)
        set(ENV{PATH} "${NCBIPTB.env.PATH};$ENV{PATH}")
    else()
        set(ENV{PATH} "${NCBIPTB.env.PATH}:$ENV{PATH}")
    endif()
endif()

#############################################################################
set(NCBI_DIRNAME_RUNTIME bin)
set(NCBI_DIRNAME_ARCHIVE lib)
if (WIN32)
    set(NCBI_DIRNAME_SHARED ${NCBI_DIRNAME_RUNTIME})
else()
    set(NCBI_DIRNAME_SHARED ${NCBI_DIRNAME_ARCHIVE})
endif()
set(NCBI_DIRNAME_SRC             src)
set(NCBI_DIRNAME_INCLUDE         include)
set(NCBI_DIRNAME_COMMON_INCLUDE  common)
set(NCBI_DIRNAME_CFGINC          inc)
set(NCBI_DIRNAME_INTERNAL        internal)
if(NCBI_PTBCFG_PACKAGING OR NCBI_PTBCFG_PACKAGED)
    set(NCBI_DIRNAME_EXPORT          res)
else()
    set(NCBI_DIRNAME_EXPORT          cmake)
endif()
set(NCBI_DIRNAME_TESTING         testing)
set(NCBI_DIRNAME_SCRIPTS         scripts)
set(NCBI_DIRNAME_COMMON_SCRIPTS  scripts/common)
set(NCBI_DIRNAME_BUILDCFG ${NCBI_DIRNAME_SRC}/build-system)
set(NCBI_DIRNAME_CMAKECFG ${NCBI_DIRNAME_SRC}/build-system/cmake)


if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/build-system/cmake/CMake.NCBIptb.cmake")
    set(_this_root     ${CMAKE_CURRENT_SOURCE_DIR}/..)
else()
    set(_this_root     ${CMAKE_SOURCE_DIR})
endif()
get_filename_component(_this_root  "${_this_root}"     ABSOLUTE)
get_filename_component(top_src_dir "${CMAKE_CURRENT_LIST_DIR}/../../.."   ABSOLUTE)

set(NCBI_TREE_ROOT    ${_this_root})
set(NCBI_SRC_ROOT     ${NCBI_TREE_ROOT}/${NCBI_DIRNAME_SRC})
set(NCBI_INC_ROOT     ${NCBI_TREE_ROOT}/${NCBI_DIRNAME_INCLUDE})
set(NCBITK_TREE_ROOT  ${top_src_dir})
set(NCBITK_SRC_ROOT   ${NCBITK_TREE_ROOT}/${NCBI_DIRNAME_SRC})
set(NCBITK_INC_ROOT   ${NCBITK_TREE_ROOT}/${NCBI_DIRNAME_INCLUDE})
if (NOT EXISTS "${NCBI_SRC_ROOT}")
    set(NCBI_SRC_ROOT   ${NCBI_TREE_ROOT})
endif()
if (NOT EXISTS "${NCBI_INC_ROOT}")
    set(NCBI_INC_ROOT   ${NCBI_TREE_ROOT})
endif()
#set(NCBI_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(build_root      ${CMAKE_BINARY_DIR})
set(builddir        ${CMAKE_BINARY_DIR})
set(includedir0     ${NCBI_INC_ROOT})
set(includedir      ${NCBI_INC_ROOT})
set(incdir          ${CMAKE_BINARY_DIR}/${NCBI_DIRNAME_CFGINC})
set(incinternal     ${NCBI_INC_ROOT}/${NCBI_DIRNAME_INTERNAL})

# After CMake language processing, \\\\\\1 becomes \\\1.
# The first \\ places a literal \ in the output. The second \1 is the placeholder that gets replaced by the match.
string(REGEX REPLACE "([][^$.\\*+?|()])" "\\\\\\1" _tmp ${CMAKE_CURRENT_SOURCE_DIR})
if (NOT DEFINED NCBI_EXTERNAL_TREE_ROOT AND NOT ${CMAKE_CURRENT_LIST_DIR} MATCHES "^${_tmp}/")
    get_filename_component(NCBI_EXTERNAL_TREE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.."   ABSOLUTE)
    message(STATUS "Found NCBI C++ Toolkit: ${NCBI_EXTERNAL_TREE_ROOT}")
endif()

set(NCBI_DIRNAME_BUILD  build)
if(NCBI_PTBCFG_PACKAGED OR NCBI_PTBCFG_PACKAGING)
    get_filename_component(NCBI_BUILD_ROOT "${CMAKE_BINARY_DIR}" ABSOLUTE)
    set(NCBI_DIRNAME_BUILD .)
elseif (DEFINED NCBI_EXTERNAL_TREE_ROOT)
    string(FIND ${CMAKE_BINARY_DIR} ${NCBI_TREE_ROOT} _pos_root)
    string(FIND ${CMAKE_BINARY_DIR} ${NCBI_SRC_ROOT}  _pos_src)
    if(NOT "${_pos_root}" LESS "0" AND "${_pos_src}" LESS "0" AND NOT "${CMAKE_BINARY_DIR}" STREQUAL "${NCBI_TREE_ROOT}")
        get_filename_component(NCBI_BUILD_ROOT "${CMAKE_BINARY_DIR}/.." ABSOLUTE)
	    get_filename_component(NCBI_DIRNAME_BUILD ${CMAKE_BINARY_DIR} NAME)
    else()
        get_filename_component(NCBI_BUILD_ROOT "${CMAKE_BINARY_DIR}" ABSOLUTE)
    endif()
else()
    get_filename_component(NCBI_BUILD_ROOT "${CMAKE_BINARY_DIR}/.." ABSOLUTE)
    get_filename_component(NCBI_DIRNAME_BUILD ${CMAKE_BINARY_DIR} NAME)
endif()

set(NCBI_CFGINC_ROOT   ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_CFGINC})

get_filename_component(incdir "${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_CFGINC}" ABSOLUTE)
if(WIN32 OR XCODE)
    set(incdir          ${incdir}/$<CONFIG>)
endif()

set(NCBI_TREE_CMAKECFG "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(NCBI_TREE_BUILDCFG "${CMAKE_CURRENT_LIST_DIR}/.."   ABSOLUTE)

if(OFF)
message("CMAKE_SOURCE_DIR    = ${CMAKE_SOURCE_DIR}")
message("NCBI_CURRENT_SOURCE_DIR   = ${NCBI_CURRENT_SOURCE_DIR}")
message("NCBI_TREE_ROOT      = ${NCBI_TREE_ROOT}")
message("NCBI_SRC_ROOT       = ${NCBI_SRC_ROOT}")
message("NCBI_INC_ROOT       = ${NCBI_INC_ROOT}")
message("NCBITK_TREE_ROOT    = ${NCBITK_TREE_ROOT}")
message("NCBITK_SRC_ROOT     = ${NCBITK_SRC_ROOT}")
message("NCBITK_INC_ROOT     = ${NCBITK_INC_ROOT}")
message("NCBI_BUILD_ROOT     = ${NCBI_BUILD_ROOT}")
message("NCBI_CFGINC_ROOT    = ${NCBI_CFGINC_ROOT}")
message("NCBI_TREE_BUILDCFG  = ${NCBI_TREE_BUILDCFG}")
message("NCBI_TREE_CMAKECFG  = ${NCBI_TREE_CMAKECFG}")
message("NCBI_EXTERNAL_TREE_ROOT  = ${NCBI_EXTERNAL_TREE_ROOT}")
endif()
