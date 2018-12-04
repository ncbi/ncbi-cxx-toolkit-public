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
set(NCBI_DIRNAME_BUILD           build)
set(NCBI_DIRNAME_EXPORT          cmake)
set(NCBI_DIRNAME_TESTING         testing)
set(NCBI_DIRNAME_BUILDCFG ${NCBI_DIRNAME_SRC}/build-system)
set(NCBI_DIRNAME_CMAKECFG ${NCBI_DIRNAME_SRC}/build-system/cmake)


set(top_src_dir     ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(abs_top_src_dir ${CMAKE_CURRENT_SOURCE_DIR}/..)
get_filename_component(top_src_dir     "${top_src_dir}"     ABSOLUTE)
get_filename_component(abs_top_src_dir "${abs_top_src_dir}" ABSOLUTE)

set(build_root      ${CMAKE_BINARY_DIR})
set(builddir        ${CMAKE_BINARY_DIR})
set(includedir0     ${top_src_dir}/${NCBI_DIRNAME_INCLUDE})
set(includedir      ${includedir0})
set(incdir          ${build_root}/${NCBI_DIRNAME_CFGINC})
set(incinternal     ${includedir0}/${NCBI_DIRNAME_INTERNAL})

set(NCBI_TREE_ROOT    ${top_src_dir})
set(NCBI_SRC_ROOT      ${CMAKE_CURRENT_SOURCE_DIR})
set(NCBI_INC_ROOT      ${includedir0})

if (DEFINED NCBI_EXTERNAL_TREE_ROOT)
    string(FIND ${CMAKE_BINARY_DIR} ${NCBI_TREE_ROOT} _pos_root)
    string(FIND ${CMAKE_BINARY_DIR} ${NCBI_SRC_ROOT}  _pos_src)
    if(NOT "${_pos_root}" LESS "0" AND "${_pos_src}" LESS "0" AND NOT "${CMAKE_BINARY_DIR}" STREQUAL "${NCBI_TREE_ROOT}")
        get_filename_component(NCBI_BUILD_ROOT "${CMAKE_BINARY_DIR}/.." ABSOLUTE)
    else()
        get_filename_component(NCBI_BUILD_ROOT "${CMAKE_BINARY_DIR}" ABSOLUTE)
    endif()
else()
    get_filename_component(NCBI_BUILD_ROOT "${CMAKE_BINARY_DIR}/.." ABSOLUTE)
endif()


set(NCBI_CFGINC_ROOT   ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_CFGINC})

if (NCBI_EXPERIMENTAL_CFG)
    get_filename_component(incdir "${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_CFGINC}" ABSOLUTE)
    if (WIN32)
        set(incdir          ${incdir}/\$\(Configuration\))
    elseif (XCODE)
        set(incdir          ${incdir}/\$\(CONFIGURATION\))
    endif()
endif()
if (NOT IS_DIRECTORY ${incinternal})
    set(incinternal     "")
endif()

if (NCBI_EXPERIMENTAL_CFG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_RUNTIME}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_SHARED}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_ARCHIVE}")
else()
    get_filename_component(EXECUTABLE_OUTPUT_PATH "${NCBI_BUILD_ROOT}/bin" ABSOLUTE)
    get_filename_component(LIBRARY_OUTPUT_PATH "${NCBI_BUILD_ROOT}/lib" ABSOLUTE)
endif()

if (DEFINED NCBI_EXTERNAL_TREE_ROOT)
    set(NCBI_TREE_BUILDCFG "${NCBI_EXTERNAL_TREE_ROOT}/${NCBI_DIRNAME_BUILDCFG}")
    set(NCBI_TREE_CMAKECFG "${NCBI_EXTERNAL_TREE_ROOT}/${NCBI_DIRNAME_CMAKECFG}")
    set(NCBI_TREE_COMMON_INCLUDE  ${NCBI_EXTERNAL_TREE_ROOT}/${NCBI_DIRNAME_INCLUDE}/common)
else()
    set(NCBI_TREE_BUILDCFG "${NCBI_TREE_ROOT}/${NCBI_DIRNAME_BUILDCFG}")
    set(NCBI_TREE_CMAKECFG "${NCBI_TREE_ROOT}/${NCBI_DIRNAME_CMAKECFG}")
    set(NCBI_TREE_COMMON_INCLUDE  ${includedir}/common)
endif()

############################################################################
# OS-specific settings
include(${NCBI_TREE_CMAKECFG}/CMakeChecks.os.cmake)

#############################################################################
# Build configurations and compiler definitions
include(${NCBI_TREE_CMAKECFG}/CMakeChecks.compiler.cmake)

#############################################################################
if (DEFINED NCBI_EXTERNAL_TREE_ROOT)
    set(_prebuilt_loc)
    if ("${NCBI_COMPILER}" STREQUAL "MSVC")
        set(_prebuilt_loc "CMake-vs")
        if ("${NCBI_COMPILER_VERSION}" LESS "1900")
            set(_prebuilt_loc ${_prebuilt_loc}2015)
        else()
            set(_prebuilt_loc ${_prebuilt_loc}2017)
        endif()
        if (BUILD_SHARED_LIBS)
            set(_prebuilt_loc ${_prebuilt_loc}/dll)
        else()
            set(_prebuilt_loc ${_prebuilt_loc}/static)
        endif()
    elseif(XCODE)
        set(_prebuilt_loc "CMake-Xcode${NCBI_COMPILER_VERSION}")
        if (BUILD_SHARED_LIBS)
            set(_prebuilt_loc ${_prebuilt_loc}/dll)
        else()
            set(_prebuilt_loc ${_prebuilt_loc}/static)
        endif()
    else()
        set(_prebuilt_loc "CMake-${NCBI_COMPILER}${NCBI_COMPILER_VERSION}-${CMAKE_BUILD_TYPE}")
        if (BUILD_SHARED_LIBS)
            set(_prebuilt_loc ${_prebuilt_loc}DLL)
        endif()
    endif()
    set(NCBI_EXTERNAL_BUILD_ROOT  ${NCBI_EXTERNAL_TREE_ROOT}/${_prebuilt_loc})

    if (IS_DIRECTORY ${NCBI_EXTERNAL_TREE_ROOT}/${NCBI_DIRNAME_INCLUDE})
        set(_ext_includedir0 ${NCBI_EXTERNAL_TREE_ROOT}/${NCBI_DIRNAME_INCLUDE})
        if (IS_DIRECTORY ${NCBI_EXTERNAL_TREE_ROOT}/${NCBI_DIRNAME_INCLUDE}/${NCBI_DIRNAME_INTERNAL})
            set(_ext_incinternal ${NCBI_EXTERNAL_TREE_ROOT}/${NCBI_DIRNAME_INCLUDE}/${NCBI_DIRNAME_INTERNAL})
        endif()
    endif()
    include_directories(${incdir} ${includedir0} ${incinternal} ${_ext_includedir0} ${_ext_incinternal})
else()
    include_directories(${incdir} ${includedir0} ${incinternal})
endif()
include_regular_expression("^.*[.](h|hpp|c|cpp|inl|inc)$")

#set(CMAKE_MODULE_PATH "${NCBI_SRC_ROOT}/build-system/cmake/" ${CMAKE_MODULE_PATH})
list(APPEND CMAKE_MODULE_PATH "${NCBI_TREE_CMAKECFG}")

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

if (EXISTS "${NCBI_TREE_BUILDCFG}/datatool_version.txt")
    FILE(STRINGS "${NCBI_TREE_BUILDCFG}/datatool_version.txt" _datatool_version)
else()
    set(_datatool_version "2.18.2")
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
set(NCBITEST_DRIVER "${NCBI_TREE_CMAKECFG}/TestDriver.cmake")
enable_testing()

#############################################################################
# Basic checks
include(${NCBI_TREE_CMAKECFG}/CMakeChecks.basic-checks.cmake)

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
include(${NCBI_TREE_CMAKECFG}/CMake.NCBIComponents.cmake)

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
                configure_file(${NCBI_TREE_CMAKECFG}/ncbiconf_msvc_site.h.in ${NCBI_CFGINC_ROOT}/${_cfg}/common/config/ncbiconf_msvc_site.h)
            elseif (XCODE)
                configure_file(${NCBI_TREE_CMAKECFG}/ncbiconf_msvc_site.h.in ${NCBI_CFGINC_ROOT}/${_cfg}/common/config/ncbiconf_xcode_site.h)
            endif()
            if (EXISTS ${NCBI_SRC_ROOT}/corelib/ncbicfg.c.in)
                configure_file(${NCBI_SRC_ROOT}/corelib/ncbicfg.c.in ${NCBI_CFGINC_ROOT}/${_cfg}/common/config/ncbicfg.cfg.c)
            endif()
            configure_file(${NCBI_TREE_COMMON_INCLUDE}/ncbi_build_ver.h.in ${NCBI_CFGINC_ROOT}/${_cfg}/common/ncbi_build_ver.h)
        endforeach()
        if(NOT EXISTS ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}/corelib/ncbicfg.c)
            file(WRITE ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}/corelib/ncbicfg.c "#include <common/config/ncbicfg.cfg.c>\n")
        endif()
    else()
#Linux
        set(c_ncbi_runpath ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
        set(SYBASE_LCL_PATH ${SYBASE_LIBRARIES})
        set(SYBASE_PATH "")

        set(NCBI_SIGNATURE "${NCBI_COMPILER}_${NCBI_COMPILER_VERSION}-${NCBI_BUILD_TYPE}--${HOST_CPU}-${HOST_OS_WITH_VERSION}-${_local_host_name}")
        configure_file(${NCBI_TREE_CMAKECFG}/config.cmake.h.in ${NCBI_CFGINC_ROOT}/ncbiconf_unix.h)
        if (EXISTS ${NCBI_SRC_ROOT}/corelib/ncbicfg.c.in)
            configure_file(${NCBI_SRC_ROOT}/corelib/ncbicfg.c.in ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}/corelib/ncbicfg.c)
        endif()

        configure_file(${NCBI_TREE_COMMON_INCLUDE}/ncbi_build_ver.h.in ${NCBI_CFGINC_ROOT}/common/ncbi_build_ver.h)
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
    configure_file(${NCBI_TREE_CMAKECFG}/config.cmake.h.in ${build_root}/inc/ncbiconf_unix.h)
    set(_os_specific_config ${build_root}/inc/ncbiconf_unix.h)
endif(UNIX)

if (WIN32)
    message(STATUS "Generating ${build_root}/inc/ncbiconf_msvc.h...")
    configure_file(${NCBI_TREE_CMAKECFG}/config.cmake.h.in ${build_root}/inc/ncbiconf_msvc.h)
    message(STATUS "Generating ${includedir}/common/config/ncbiconf_msvc_site.h...")
    configure_file(${NCBI_TREE_CMAKECFG}/ncbiconf_msvc_site.h.in ${includedir}/common/config/ncbiconf_msvc_site.h)
    set(_os_specific_config ${build_root}/inc/ncbiconf_msvc.h ${includedir}/common/config/ncbiconf_msvc_site.h)
endif (WIN32)

if (APPLE AND NOT UNIX) #XXX 
    message(STATUS "Generating ${build_root}/inc/ncbiconf_xcode.h...")
    configure_file(${NCBI_TREE_CMAKECFG}/config.cmake.h.in ${build_root}/inc/ncbiconf_xcode.h)
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
include(${NCBI_TREE_CMAKECFG}/CMakeChecks.final-message.cmake)

