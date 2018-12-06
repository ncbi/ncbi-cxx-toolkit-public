#############################################################################
# $Id$
#############################################################################

if("${CMAKE_GENERATOR}" STREQUAL "Xcode")
    if(NOT DEFINED XCODE)
        set(XCODE ON)
    endif()
endif()

###############################################################################
# must be set to OFF on trunk
if(NOT DEFINED NCBI_EXPERIMENTAL)
    if (DEFINED NCBI_EXTERNAL_TREE_ROOT)
        set(NCBI_EXPERIMENTAL ON)
    else()
        set(NCBI_EXPERIMENTAL OFF)
    endif()
endif()

if("${NCBI_PTBCFG_PROJECT_LIST}" STREQUAL "")
    unset(NCBI_PTBCFG_PROJECT_LIST)
endif()
if("${NCBI_PTBCFG_PROJECT_TAGS}" STREQUAL "")
    unset(NCBI_PTBCFG_PROJECT_TAGS)
endif()
if("${NCBI_PTBCFG_PROJECT_TARGETS}" STREQUAL "")
    unset(NCBI_PTBCFG_PROJECT_TARGETS)
endif()

#  set(NCBI_PTBCFG_PROJECT_TAGS    *;-test)
#  set(NCBI_PTBCFG_PROJECT_TARGETS   datatool;xcgi$)
#  set(NCBI_PTBCFG_PROJECT_LIST     corelib serial build-system)
#  set(NCBI_VERBOSE_ALLPROJECTS OFF)
#  set(NCBI_VERBOSE_PROJECTS xncbi variation_utils)

if(DEFINED NCBI_VERBOSE_PROJECTS)
    foreach(_prj IN LISTS NCBI_VERBOSE_PROJECTS)
        set(NCBI_VERBOSE_PROJECT_${_prj}   ON)
    endforeach()
endif()

cmake_policy(SET CMP0057 NEW)
if(NCBI_EXPERIMENTAL)
    cmake_policy(SET CMP0054 NEW)

    set(NCBI_EXPERIMENTAL_CFG              ON)
    set(NCBI_EXPERIMENTAL_SUBDIRS          ON)
    set(NCBI_EXPERIMENTAL_DISABLE_HUNTER   ON)
    set(NCBI_VERBOSE_ALLPROJECTS           OFF)

    if(BUILD_SHARED_LIBS)
        if(WIN32 OR XCODE)
            set(NCBI_PTBCFG_COMPOSITE_DLL dll)
        endif()
    endif()

if(OFF)
    if (DEFINED NCBI_PTBCFG_COMPOSITE_DLL
        OR DEFINED NCBI_EXTERNAL_TREE_ROOT
        OR DEFINED NCBI_VERBOSE_PROJECTS
        OR NOT "${NCBI_PTBCFG_PROJECT_LIST}" STREQUAL ""
        OR NOT "${NCBI_PTBCFG_PROJECT_TAGS}" STREQUAL ""
        OR NOT "${NCBI_PTBCFG_PROJECT_TARGETS}" STREQUAL "")
        set(NCBI_PTBCFG_ENABLE_COLLECTOR ON)
    endif()
else()
        set(NCBI_PTBCFG_ENABLE_COLLECTOR ON)
endif()

    if (NOT "${NCBI_PTBCFG_INSTALL_PATH}" STREQUAL "")
        set(NCBI_PTBCFG_DOINSTALL              ON)
        string(REPLACE "\\" "/" NCBI_PTBCFG_INSTALL_PATH ${NCBI_PTBCFG_INSTALL_PATH})
        set(CMAKE_INSTALL_PREFIX "${NCBI_PTBCFG_INSTALL_PATH}" CACHE STRING "Reset the installation destination" FORCE)
        set(NCBI_PTBCFG_INSTALL_TAGS "*;-test;-demo")
    endif()
    set(NCBI_PTBCFG_INSTALL_EXPORT ncbi-cpp-toolkit)

else()
    cmake_policy(SET CMP0054 OLD)

    set(NCBI_EXPERIMENTAL_CFG              OFF)
    set(NCBI_EXPERIMENTAL_SUBDIRS          OFF)
    set(NCBI_EXPERIMENTAL_DISABLE_HUNTER   OFF)
    set(NCBI_VERBOSE_ALLPROJECTS           OFF)
    set(NCBI_PTBCFG_ENABLE_COLLECTOR       OFF)
    set(NCBI_PTBCFG_DOINSTALL              OFF)
endif()

if(DEFINED NCBI_PTBCFG_PROJECT_LIST AND EXISTS "${NCBI_PTBCFG_PROJECT_LIST}")
    if (NOT IS_DIRECTORY "${NCBI_PTBCFG_PROJECT_LIST}")
        file(STRINGS "${NCBI_PTBCFG_PROJECT_LIST}" NCBI_PTBCFG_PROJECT_LIST)
    endif()
endif()

if(DEFINED NCBI_PTBCFG_PROJECT_TAGS AND EXISTS "${NCBI_PTBCFG_PROJECT_TAGS}")
    if (NOT IS_DIRECTORY "${NCBI_PTBCFG_PROJECT_TAGS}")
        file(STRINGS "${NCBI_PTBCFG_PROJECT_TAGS}" NCBI_PTBCFG_PROJECT_TAGS)
    endif()
endif()

if(DEFINED NCBI_PTBCFG_PROJECT_TARGETS AND EXISTS "${NCBI_PTBCFG_PROJECT_TARGETS}")
    if (NOT IS_DIRECTORY "${NCBI_PTBCFG_PROJECT_TARGETS}")
        file(STRINGS "${NCBI_PTBCFG_PROJECT_TARGETS}" NCBI_PTBCFG_PROJECT_TARGETS)
    endif()
endif()

###############################################################################
if (OFF)
message("CMAKE_CONFIGURATION_TYPES: ${CMAKE_CONFIGURATION_TYPES}")
set(_cfg_types Debug Release MinSizeRel RelWithDebInfo)

message("")
message("CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
foreach(_cfg ${_cfg_types})
  string(TOUPPER ${_cfg} _upname)
  message("CMAKE_C_FLAGS_${_upname}: ${CMAKE_C_FLAGS_${_upname}}")
endforeach()

message("")
message("CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
foreach(_cfg ${_cfg_types})
  string(TOUPPER ${_cfg} _upname)
  message("CMAKE_CXX_FLAGS_${_upname}: ${CMAKE_CXX_FLAGS_${_upname}}")
endforeach()

message("")
message("CMAKE_STATIC_LINKER_FLAGS: ${CMAKE_STATIC_LINKER_FLAGS}")
foreach(_cfg ${_cfg_types})
  string(TOUPPER ${_cfg} _upname)
  message("CMAKE_STATIC_LINKER_FLAGS_${_upname}: ${CMAKE_STATIC_LINKER_FLAGS_${_upname}}")
endforeach()

message("")
message("CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
foreach(_cfg ${_cfg_types})
  string(TOUPPER ${_cfg} _upname)
  message("CMAKE_EXE_LINKER_FLAGS_${_upname}: ${CMAKE_EXE_LINKER_FLAGS_${_upname}}")
endforeach()

message("")
message("CMAKE_MODULE_LINKER_FLAGS: ${CMAKE_MODULE_LINKER_FLAGS}")
foreach(_cfg ${_cfg_types})
  string(TOUPPER ${_cfg} _upname)
  message("CMAKE_MODULE_LINKER_FLAGS_${_upname}: ${CMAKE_MODULE_LINKER_FLAGS_${_upname}}")
endforeach()

message("")
message("CMAKE_SHARED_LINKER_FLAGS: ${CMAKE_SHARED_LINKER_FLAGS}")
foreach(_cfg ${_cfg_types})
  string(TOUPPER ${_cfg} _upname)
  message("CMAKE_SHARED_LINKER_FLAGS_${_upname}: ${CMAKE_SHARED_LINKER_FLAGS_${_upname}}")
endforeach()
#return()
endif()

###############################################################################
## Initialize Hunter
##
if (NOT NCBI_EXPERIMENTAL_DISABLE_HUNTER)
if (WIN32)
    if (NOT HUNTER_ROOT)
        set(HUNTER_ROOT ${CMAKE_SOURCE_DIR}/../HunterPackages)
    endif()
    include(build-system/cmake/HunterGate.cmake)
    HunterGate(
        URL "https://github.com/ruslo/hunter/archive/v0.18.39.tar.gz"
        SHA1 "a6fbc056c3d9d7acdaa0a07c575c9352951c2f6c"
    )
endif()
endif()

if (DEFINED NCBI_EXTERNAL_TREE_ROOT)
    include(${NCBI_EXTERNAL_TREE_ROOT}/src/build-system/cmake/CMakeMacros.cmake)
    include(${NCBI_EXTERNAL_TREE_ROOT}/src/build-system/cmake/CMake.NCBIptb.cmake)
    include(${NCBI_EXTERNAL_TREE_ROOT}/src/build-system/cmake/CMakeChecks.cmake)
    if (EXISTS ${NCBI_EXTERNAL_BUILD_ROOT}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}.cmake)
        include(${NCBI_EXTERNAL_BUILD_ROOT}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}.cmake)
    else()
        message(FATAL_ERROR "${NCBI_PTBCFG_INSTALL_EXPORT} was not found in ${NCBI_EXTERNAL_BUILD_ROOT}/${NCBI_DIRNAME_EXPORT}")
    endif()
    NCBI_internal_import_hostinfo(${NCBI_EXTERNAL_BUILD_ROOT}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}.hostinfo)
else()
    include(build-system/cmake/CMakeMacros.cmake)
    include(build-system/cmake/CMake.NCBIptb.cmake)
    include(build-system/cmake/CMakeChecks.cmake)
endif()
