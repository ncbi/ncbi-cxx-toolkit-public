#############################################################################
# $Id$
#############################################################################
#
# Note:
# This file is included before everything else
# Anything related to the initial state should go early in this file!


if("${CMAKE_GENERATOR}" STREQUAL "Xcode")
    if(NOT DEFINED XCODE)
        set(XCODE ON)
    endif()
endif()

#############################################################################
# Source tree description
#
set(top_src_dir     ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(abs_top_src_dir ${CMAKE_CURRENT_SOURCE_DIR}/..)

get_filename_component(top_src_dir     "${top_src_dir}"     ABSOLUTE)
get_filename_component(abs_top_src_dir "${abs_top_src_dir}" ABSOLUTE)

set(build_root      ${CMAKE_BINARY_DIR})
set(builddir        ${CMAKE_BINARY_DIR})
set(includedir0     ${top_src_dir}/include)
set(includedir      ${includedir0})
set(incdir          ${build_root}/inc)
set(incinternal     ${includedir0}/internal)

if (NCBI_EXPERIMENTAL_CFG)
    get_filename_component(incdir "${build_root}/../inc" REALPATH)
    if (WIN32)
        set(incdir          ${incdir}/\$\(Configuration\))
    elseif (XCODE)
        set(incdir          ${incdir}/\$\(CONFIGURATION\))
    endif()
endif()
if (NOT IS_DIRECTORY ${incinternal})
    set(incinternal     "")
endif()
set(NCBI_BUILD_ROOT ${CMAKE_BINARY_DIR})
set(NCBI_SRC_ROOT   ${CMAKE_CURRENT_SOURCE_DIR})
set(NCBI_INC_ROOT   ${includedir0})

get_filename_component(top_src_dir "${top_src_dir}" REALPATH)
get_filename_component(abs_top_src_dir "${abs_top_src_dir}" REALPATH)
get_filename_component(build_root "${build_root}" REALPATH)
get_filename_component(includedir "${includedir}" REALPATH)

if (NCBI_EXPERIMENTAL_CFG)
    get_filename_component(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${NCBI_BUILD_ROOT}/../bin" REALPATH)
    if (WIN32)
        get_filename_component(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${NCBI_BUILD_ROOT}/../bin" REALPATH)
    else()
        get_filename_component(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${NCBI_BUILD_ROOT}/../lib" REALPATH)
    endif()
    get_filename_component(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${NCBI_BUILD_ROOT}/../lib" REALPATH)
else()
    get_filename_component(EXECUTABLE_OUTPUT_PATH "${NCBI_BUILD_ROOT}/../bin" REALPATH)
    get_filename_component(LIBRARY_OUTPUT_PATH "${NCBI_BUILD_ROOT}/../lib" REALPATH)
endif()

include_directories(${incdir} ${includedir0} ${incinternal})

set(CMAKE_MODULE_PATH "${NCBI_SRC_ROOT}/build-system/cmake/" ${CMAKE_MODULE_PATH})

##############################################################################
# Find datatool app

if (WIN32)
    set(NCBI_DATATOOL_BASE "//snowman/win-coremake/App/Ncbi/cppcore/datatool/msvc")
elseif(XCODE)
    set(NCBI_DATATOOL_BASE "/net/snowman/vol/export2/win-coremake/App/Ncbi/cppcore/datatool/XCode")
else()
#FIXME: Not just Linux!
    set(NCBI_DATATOOL_BASE "/net/snowman/vol/export2/win-coremake/App/Ncbi/cppcore/datatool/Linux64")
endif()

if (EXISTS "${NCBI_SRC_ROOT}/build-system/datatool_version.txt")
    FILE(READ "${NCBI_SRC_ROOT}/build-system/datatool_version.txt" _datatool_version)
    string(REGEX MATCH "[0-9][0-9.]*[0-9]" _datatool_version "${_datatool_version}")
else()
    set(_datatool_version "2.17.0")
    message(WARNING "Failed to find datatool_version.txt, defaulting to version ${_datatool_version})")
endif()
message(STATUS "Datatool version required by software: ${_datatool_version}")

if (WIN32)
    set(NCBI_DATATOOL_BIN "datatool.exe")
else()
    set(NCBI_DATATOOL_BIN "datatool")
endif()

if (EXISTS "${NCBI_DATATOOL_BASE}/${_datatool_version}/${NCBI_DATATOOL_BIN}")
    set (NCBI_DATATOOL "${NCBI_DATATOOL_BASE}/${_datatool_version}/${NCBI_DATATOOL_BIN}")
    message(STATUS "Datatool location: ${NCBI_DATATOOL}")
else()
    if (NCBI_EXPERIMENTAL_CFG)
        set (NCBI_DATATOOL datatool)
    else()
        set (NCBI_DATATOOL $<TARGET_FILE:datatool-app>)
    endif()
    message(STATUS "Datatool location: <locally compiled>")
endif()

#############################################################################
# Testing
enable_testing()

############################################################################
# OS-specific settings
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.os.cmake)

#############################################################################
# Build configurations and compiler definitions
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.compiler.cmake)

#############################################################################
# Basic checks
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.basic-checks.cmake)

#############################################################################
# Hunter packages for Windows

if (NOT NCBI_EXPERIMENTAL_DISABLE_HUNTER)
if (WIN32)
    #set(HUNTER_STATUS_DEBUG TRUE)
    hunter_add_package(wxWidgets)
    hunter_add_package(Boost COMPONENTS filesystem regex system test)
    hunter_add_package(ZLIB)
    hunter_add_package(BZip2)
    hunter_add_package(Jpeg)
    hunter_add_package(PNG)
    hunter_add_package(TIFF)
    #hunter_add_package(freetype)
endif()
endif()

#############################################################################
# External libraries
include(${top_src_dir}/src/build-system/cmake/CMake.NCBIComponents.cmake)

#############################################################################
# Generation of configuration files

# Stable components
# This sets a version to be used throughout our config process
# NOTE: Adjust as needed
#
set(NCBI_CPP_TOOLKIT_VERSION_MAJOR 21)
set(NCBI_CPP_TOOLKIT_VERSION_MINOR 0)
set(NCBI_CPP_TOOLKIT_VERSION_PATCH 0)
set(NCBI_CPP_TOOLKIT_VERSION_EXTRA "")
set(NCBI_CPP_TOOLKIT_VERSION
    ${NCBI_CPP_TOOLKIT_VERSION_MAJOR}.${NCBI_CPP_TOOLKIT_VERSION_MINOR}.${NCBI_CPP_TOOLKIT_VERSION_PATCH}${NCBI_CPP_TOOLKIT_VERSION_EXTRA})

#############################################################################
# Subversion
# This is needed for some use cases

if (EXISTS ${top_src_dir}/.svn)
    include(FindSubversion)
    Subversion_WC_INFO(${top_src_dir} TOOLKIT)
    Subversion_WC_INFO(${top_src_dir}/src/corelib CORELIB)
else()
    set(TOOLKIT_WC_REVISION 0)
    set(TOOLKIT_WC_URL "")
    set(CORELIB_WC_REVISION 0)
    set(CORELIB_WC_URL "")
endif()

set(NCBI_SUBVERSION_REVISION ${TOOLKIT_WC_REVISION})
message(STATUS "SVN revision = ${TOOLKIT_WC_REVISION}")
message(STATUS "SVN URL = ${TOOLKIT_WC_URL}")

if ("$ENV{NCBI_TEAMCITY_BUILD_NUMBER}" STREQUAL "")
    set(NCBI_TEAMCITY_BUILD_NUMBER 0)
    message(STATUS "TeamCity build number = ${NCBI_TEAMCITY_BUILD_NUMBER}")
endif()

set(NCBI_SC_VERSION 0)
if (NOT "${CORELIB_WC_URL}" STREQUAL "")
    string(REGEX REPLACE ".*/production/components/core/([0-9]+)\\.[0-9]+/.*" "\\1" _SC_VER "${CORELIB_WC_URL}")
    string(LENGTH "${_SC_VER}" _SC_VER_LEN)
    if (${_SC_VER_LEN} LESS 10 AND NOT "${_SC_VER}" STREQUAL "")
        set(NCBI_SC_VERSION ${_SC_VER})
        message(STATUS "Stable Components Number = ${NCBI_SC_VERSION}")
    endif()
endif()

#############################################################################
cmake_host_system_information(RESULT _local_host_name  QUERY HOSTNAME)
if (WIN32 OR XCODE)
    set(HOST "${HOST_CPU}-${HOST_OS}")
else()
#    set(HOST "${HOST_CPU}-unknown-${HOST_OS}")
    set(HOST "${HOST_CPU}-${HOST_OS}")
endif()
set(FEATURES "${NCBI_ALL_COMPONENTS}")

if (NCBI_EXPERIMENTAL_CFG)

    if (WIN32 OR XCODE)
        foreach(_cfg ${CMAKE_CONFIGURATION_TYPES})

            set(_file "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_cfg}")
            if (WIN32)
                string(REPLACE "/" "\\\\" _file ${_file})
            endif()
            set(c_ncbi_runpath "${_file}")
            if (WIN32)
                string(REPLACE "/" "\\\\" SYBASE_PATH ${SYBASE_PATH})
            endif()

            set(NCBI_SIGNATURE "${NCBI_COMPILER}_${NCBI_COMPILER_VERSION}-${_cfg}--${HOST}-${_local_host_name}")
            if (WIN32)
                configure_file(${NCBI_SRC_ROOT}/build-system/cmake/ncbiconf_msvc_site.h.in ${NCBI_BUILD_ROOT}/../inc/${_cfg}/common/config/ncbiconf_msvc_site.h)
            elseif (XCODE)
                configure_file(${NCBI_SRC_ROOT}/build-system/cmake/ncbiconf_msvc_site.h.in ${NCBI_BUILD_ROOT}/../inc/${_cfg}/common/config/ncbiconf_xcode_site.h)
            endif()
            configure_file(${NCBI_SRC_ROOT}/corelib/ncbicfg.c.in ${NCBI_BUILD_ROOT}/../inc/${_cfg}/common/config/ncbicfg.cfg.c)
        endforeach()
        if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/corelib/ncbicfg.c)
            file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/corelib/ncbicfg.c "#include <common/config/ncbicfg.cfg.c>\n")
        endif()
        return()
    else()
#Linux
        set(c_ncbi_runpath ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
        set(SYBASE_LCL_PATH ${SYBASE_LIBRARIES})
        set(SYBASE_PATH "")

        set(NCBI_SIGNATURE "${NCBI_COMPILER}_${NCBI_COMPILER_VERSION}-${NCBI_BUILD_TYPE}--${HOST_CPU}-${HOST_OS_WITH_VERSION}-${_local_host_name}")
        configure_file(${NCBI_SRC_ROOT}/build-system/cmake/config.cmake.h.in ${NCBI_BUILD_ROOT}/../inc/ncbiconf_unix.h)
        configure_file(${NCBI_SRC_ROOT}/corelib/ncbicfg.c.in ${NCBI_BUILD_ROOT}/corelib/ncbicfg.c)
    endif()

else (NCBI_EXPERIMENTAL_CFG)

set(HOST "${HOST_CPU}-unknown-${HOST_OS}")
set(NCBI_SIGNATURE "${CMAKE_C_COMPILER_ID}_${MSVC_VERSION}-${HOST}-${_local_host_name}")

if (WIN32)
    set(HOST "${HOST_CPU}-${HOST_OS}")
    set(NCBI_SIGNATURE "${CMAKE_C_COMPILER_ID}_${MSVC_VERSION}-${HOST}-${_local_host_name}")
endif()

# This file holds information about the build version
message(STATUS "Generating ${includedir}/common/ncbi_build_ver.h")
configure_file(${includedir}/common/ncbi_build_ver.h.in ${includedir}/common/ncbi_build_ver.h)

# OS-specific generated header configs
if (UNIX)
    message(STATUS "Generating ${build_root}/inc/ncbiconf_unix.h...")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build-system/cmake/config.cmake.h.in ${build_root}/inc/ncbiconf_unix.h)
    set(_os_specific_config ${build_root}/inc/ncbiconf_unix.h)
endif(UNIX)

if (WIN32)
    message(STATUS "Generating ${build_root}/inc/ncbiconf_msvc.h...")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build-system/cmake/config.cmake.h.in ${build_root}/inc/ncbiconf_msvc.h)
    message(STATUS "Generating ${includedir}/common/config/ncbiconf_msvc_site.h...")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build-system/cmake/ncbiconf_msvc_site.h.in ${includedir}/common/config/ncbiconf_msvc_site.h)
    set(_os_specific_config ${build_root}/inc/ncbiconf_msvc.h ${includedir}/common/config/ncbiconf_msvc_site.h)
endif (WIN32)

if (APPLE AND NOT UNIX) #XXX 
    message(STATUS "Generating ${build_root}/inc/ncbiconf_xcode.h...")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build-system/cmake/config.cmake.h.in ${build_root}/inc/ncbiconf_xcode.h)
    set(_os_specific_config ${build_root}/inc/ncbiconf_xcode.h)
endif (APPLE AND NOT UNIX)

#
# write ncbicfg.c.in
#
# FIXME:
# We need to set these variables to get them into the cfg file:
#  - c_ncbi_runpath
#  - SYBASE_LCL_PATH
#  - SYBASE_PATH
#  - FEATURES
set(c_ncbi_runpath "$ORIGIN/../lib")
set(SYBASE_LCL_PATH ${SYBASE_LIBRARIES})
set(SYBASE_PATH "")
set(FEATURES "")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/corelib/ncbicfg.c.in ${CMAKE_BINARY_DIR}/corelib/ncbicfg.c)

endif (NCBI_EXPERIMENTAL_CFG)

#
# Dump our final diagnostics
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.final-message.cmake)

