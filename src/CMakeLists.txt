#############################################################################
# $Id$
#############################################################################
##############################################################################

# We require at least cmake-3.3
cmake_minimum_required(VERSION 3.3)

if (NOT DEFINED NCBI_CMAKEPROJECT_NAME)
  set(NCBI_CMAKEPROJECT_NAME CPP)
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
  app
  sample
)

project(${NCBI_CMAKEPROJECT_NAME})

include(build-system/cmake/CMake.NCBItoolkit.cmake)

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

NCBI_add_root_subdirectory( ${NCBI_PTBCFG_KNOWN_FOLDERS})

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
