#############################################################################
# $Id$
#############################################################################
##############################################################################

# We require at least cmake-3.0
cmake_minimum_required(VERSION 3.0)

if("${CMAKE_GENERATOR}" STREQUAL "Xcode")
    if(NOT DEFINED XCODE)
        set(XCODE ON)
    endif()
endif()

###############################################################################
# must be set to OFF on trunk
if(NOT DEFINED NCBI_EXPERIMENTAL)
  set(NCBI_EXPERIMENTAL OFF)
endif()

if (NOT DEFINED NCBI_CMAKEPROJECT_NAME)
  set(NCBI_CMAKEPROJECT_NAME CPP)
endif()

#  set(NCBI_PTBCFG_PROJECT_LIST     corelib serial build-system)
#  set(NCBI_PTBCFG_ALLOWED_PROJECTS project_tree_builder datatool)
#  set(NCBI_VERBOSE_ALLPROJECTS OFF)
#  set(NCBI_VERBOSE_PROJECTS xncbi variation_utils)

if(DEFINED NCBI_VERBOSE_PROJECTS)
  foreach(_prj IN LISTS NCBI_VERBOSE_PROJECTS)
    set(NCBI_VERBOSE_PROJECT_${_prj}   ON)
  endforeach()
endif()

set(NCBI_PTBCFG_KNOWN_FOLDERS
  corelib
  util
  connect
  cgi
  html
  build-system
  serial
  db
  dbapi
  objects
  objmgr
  objtools
  ctools
  algo
  misc
  sra
)


if(NCBI_EXPERIMENTAL)
  cmake_policy(SET CMP0054 NEW)

  set(NCBI_EXPERIMENTAL_CFG              ON)
  set(NCBI_EXPERIMENTAL_SUBDIRS          ON)
  set(NCBI_EXPERIMENTAL_DISABLE_HUNTER   ON)

  if(BUILD_SHARED_LIBS)
    if(WIN32 OR XCODE)
      set(NCBI_PTBCFG_COMPOSITE_DLL dll)
    endif()
  endif()

  if (DEFINED NCBI_PTBCFG_COMPOSITE_DLL OR DEFINED NCBI_PTBCFG_ALLOWED_PROJECTS OR DEFINED NCBI_PTBCFG_PROJECT_LIST)
    set(NCBI_PTBCFG_ENABLE_COLLECTOR ON)
  endif()

else()
  cmake_policy(SET CMP0054 OLD)

  set(NCBI_EXPERIMENTAL_CFG              OFF)
  set(NCBI_EXPERIMENTAL_SUBDIRS          OFF)
  set(NCBI_EXPERIMENTAL_DISABLE_HUNTER   OFF)
  set(NCBI_EXPERIMENTAL_VERBOSE          OFF)
  set(NCBI_PTBCFG_ENABLE_COLLECTOR       OFF)
endif()

if(DEFINED NCBI_PTBCFG_PROJECT_LIST AND EXISTS "${NCBI_PTBCFG_PROJECT_LIST}")
  if (NOT IS_DIRECTORY "${NCBI_PTBCFG_PROJECT_LIST}")
    file(STRINGS "${NCBI_PTBCFG_PROJECT_LIST}" NCBI_PTBCFG_PROJECT_LIST)
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


project(${NCBI_CMAKEPROJECT_NAME})

include(build-system/cmake/CMakeMacros.cmake)
include(build-system/cmake/CMake.NCBIptb.cmake)
include(build-system/cmake/CMakeChecks.cmake)

if (NOT NCBI_EXPERIMENTAL_CFG)
#
# NOTE NOTE NOTE
#
# The macros below are deliberately added here
# Including these in CMakeMacros.cmake causes these overrides to be used from
# within all package configuration processes
# That will result in tons of errors related to the use of add_library() from
# within other package specifications (i.e., Threads, Boost)
#
# LEAVE THESE HERE
#

##############################################################################
##
## Macro to force installation (recursively) for all include files in a
## directory
##
## Note: we call this with all top-level directories!
##

macro(ncbi_install_includes tag)
    #message(STATUS "Installing headers: ${tag}")
    install(
        DIRECTORY ${top_src_dir}/include/${tag}
        DESTINATION include
        COMPONENT   Devel
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "*.h"
        PATTERN "*.inc"
        PATTERN "*.inl"
        )
endmacro(ncbi_install_includes)


##############################################################################
##
## Override of add_library:
## We do this to force installation without having to explicitly add
## installation for all libraries

macro(add_library _libname)
    #message(STATUS "Adding library: ${_libname}")
    _add_library(${_libname} ${ARGN})
    install(
        TARGETS ${_libname}
        DESTINATION lib
        EXPORT ncbi-cpp-toolkit
        )
endmacro(add_library)


macro(add_executable _exename)
    #message(STATUS "Adding executable: ${_exename}")
    _add_executable(${_exename} ${ARGN})
    install(
        TARGETS ${_exename}
        DESTINATION bin
        EXPORT ncbi-cpp-toolkit
        )
endmacro(add_executable)

endif()
#
##############################################################################
#

# Recurse subdirectories
if (NCBI_EXPERIMENTAL_SUBDIRS)

NCBI_add_root_subdirectory( ${NCBI_PTBCFG_COMPOSITE_DLL}  ${NCBI_PTBCFG_KNOWN_FOLDERS})

else (NCBI_EXPERIMENTAL_SUBDIRS)

add_subdirectory(corelib)
add_subdirectory(util)
add_subdirectory(connect)
add_subdirectory(cgi)
add_subdirectory(html)
add_subdirectory(build-system)
add_subdirectory(serial)
add_subdirectory(db)
add_subdirectory(dbapi)
add_subdirectory(objects)
add_subdirectory(objmgr)
add_subdirectory(objtools)
add_subdirectory(ctools)
add_subdirectory(algo)
add_subdirectory(misc)
add_subdirectory(sra)
add_subdirectory(app)

add_subdirectory_optional(gui)
add_subdirectory_optional(sample)
add_subdirectory_optional(internal)

ncbi_install_includes(algo)
ncbi_install_includes(app)
ncbi_install_includes(build-system)
ncbi_install_includes(cgi)
ncbi_install_includes(common)
ncbi_install_includes(connect)
ncbi_install_includes(corelib)
ncbi_install_includes(ctools)
ncbi_install_includes(db)
ncbi_install_includes(dbapi)
ncbi_install_includes(gui)
ncbi_install_includes(html)
ncbi_install_includes(internal)
ncbi_install_includes(misc)
ncbi_install_includes(objects)
ncbi_install_includes(objmgr)
ncbi_install_includes(objtools)
ncbi_install_includes(serial)
ncbi_install_includes(sra)
ncbi_install_includes(util)


##############################################################################
#
# Special-purpose files to install
# There are just a few of these
#
install(
    FILES ${top_src_dir}/include/ncbi_pch.hpp
          ${top_src_dir}/include/ncbi_source_ver.h
          ${top_src_dir}/include/ncbiconf.h
    DESTINATION include
    COMPONENT   Devel
    )

install(
    FILES ${_os_specific_config}
    DESTINATION lib/cmake/ncbi-cpp-toolkit-${NCBI_CPP_TOOLKIT_VERSION}/include
    COMPONENT   Devel
    )


#
##############################################################################
#

#
# Package config and setup issues
# NOTE: This must come last!!
#
# This code is needed to make sure we can import the toolkit when the toolkit
# is packaged as a third-party component
#

include(CMakePackageConfigHelpers)

set(PACKAGE_INCLUDE_INSTALL_DIRS include)
set(PACKAGE_SITE_INCLUDE_INSTALL_DIRS lib/cmake/ncbi-cpp-toolkit-${NCBI_CPP_TOOLKIT_VERSION}/include)
set(PACKAGE_LIBRARY_INSTALL_DIRS lib)

# Compiler definitions require some magic
get_directory_property(Defs COMPILE_DEFINITIONS)
foreach (d ${Defs} )
    set(PACKAGE_COMPILE_DEFINITIONS "${PACKAGE_COMPILE_DEFINITIONS} -D${d}")
endforeach()


configure_package_config_file(
    "build-system/cmake/NCBI-ToolkitConfig.cmake.in"
    ${CMAKE_CURRENT_BINARY_DIR}/ncbi-cpp-toolkit-config.cmake
    INSTALL_DESTINATION lib/cmake/ncbi-cpp-toolkit-${NCBI_CPP_TOOLKIT_VERSION}
    PATH_VARS PACKAGE_INCLUDE_INSTALL_DIRS PACKAGE_LIBRARY_INSTALL_DIRS
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ncbi-cpp-toolkit-config-version.cmake
    VERSION ${NCBI_CPP_TOOLKIT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/ncbi-cpp-toolkit-config.cmake
          ${CMAKE_CURRENT_BINARY_DIR}/ncbi-cpp-toolkit-config-version.cmake
    DESTINATION lib/cmake/ncbi-cpp-toolkit-${NCBI_CPP_TOOLKIT_VERSION}
)

install(EXPORT ncbi-cpp-toolkit
    DESTINATION lib/cmake/ncbi-cpp-toolkit-${NCBI_CPP_TOOLKIT_VERSION}
    )

endif (NCBI_EXPERIMENTAL_SUBDIRS)
