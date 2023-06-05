#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper
##    Author: Andrei Gourianov, gouriano@ncbi
##
##  Checks and configuration


############################################################################
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_RUNTIME}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_SHARED}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_ARCHIVE}")

if(EXISTS ${NCBI_TREE_ROOT}/CMake.CustomConfig.txt)
	include(${NCBI_TREE_ROOT}/CMake.CustomConfig.txt)
endif()

############################################################################
# OS-specific settings
include(${NCBI_TREE_CMAKECFG}/CMakeChecks.os.cmake)

#############################################################################
# Build configurations and compiler definitions
include(${NCBI_TREE_CMAKECFG}/CMakeChecks.compiler.cmake)

#############################################################################
if ("${NCBI_COMPILER}" STREQUAL "MSVC")
    if ("${NCBI_COMPILER_VERSION}" LESS "1910")
        set(_msvc_year 2015)
    elseif ("${NCBI_COMPILER_VERSION}" LESS "1920")
        set(_msvc_year 2017)
    elseif ("${NCBI_COMPILER_VERSION}" LESS "1930")
        set(_msvc_year 2019)
    else()
        set(_msvc_year 2022)
    endif()
    set(NCBI_COMPILER_ALT "VS_${_msvc_year}")
endif()
if(NOT DEFINED NCBI_COMPILER_PREBUILT)
    if ("${NCBI_COMPILER}" STREQUAL "MSVC")
        set(NCBI_COMPILER_PREBUILT "VS${_msvc_year}")
    elseif(XCODE)
        set(NCBI_COMPILER_PREBUILT "Xcode${NCBI_COMPILER_VERSION}")
    else()
        set(NCBI_COMPILER_PREBUILT "${NCBI_COMPILER}${NCBI_COMPILER_VERSION}")
    endif()
endif()
if(NOT DEFINED NCBI_DIRNAME_PREBUILT)
    set(NCBI_DIRNAME_PREBUILT "CMake-${NCBI_COMPILER_PREBUILT}")
    if ("${NCBI_COMPILER}" STREQUAL "MSVC" OR XCODE)
        set(NCBI_DIRNAME_PREBUILT ${NCBI_DIRNAME_PREBUILT}${NCBI_PTBCFG_INSTALL_SUFFIX})
        if (BUILD_SHARED_LIBS)
            set(NCBI_DIRNAME_PREBUILT ${NCBI_DIRNAME_PREBUILT}-DLL)
        endif()
    else()
        set(NCBI_DIRNAME_PREBUILT ${NCBI_DIRNAME_PREBUILT}-${STD_BUILD_TYPE})
        set(NCBI_DIRNAME_PREBUILT ${NCBI_DIRNAME_PREBUILT}${NCBI_PTBCFG_INSTALL_SUFFIX})
        if (BUILD_SHARED_LIBS)
            set(NCBI_DIRNAME_PREBUILT ${NCBI_DIRNAME_PREBUILT}DLL)
        endif()
    endif()
endif()

set(_tk_includedir      ${NCBITK_INC_ROOT})
set(_tk_incinternal     ${NCBITK_INC_ROOT}/${NCBI_DIRNAME_INTERNAL})
set(_inc_dirs)
foreach( _inc IN ITEMS ${includedir} ${incinternal} ${_tk_includedir} ${_tk_incinternal})
    if (IS_DIRECTORY ${_inc})
        list(APPEND _inc_dirs ${_inc})
    endif()
endforeach()
list(REMOVE_DUPLICATES _inc_dirs)
include_directories(${incdir} ${_inc_dirs})
include_regular_expression("^.*[.](h|hpp|c|cpp|inl|inc)$")
if(OFF)
    message("include_directories(${incdir} ${_inc_dirs})")
endif()

if (DEFINED NCBI_EXTERNAL_TREE_ROOT)
    set(NCBI_EXTERNAL_BUILD_ROOT  ${NCBI_EXTERNAL_TREE_ROOT}/${NCBI_DIRNAME_PREBUILT})
    if (NOT EXISTS ${NCBI_EXTERNAL_BUILD_ROOT}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}.cmake)
        message(FATAL_ERROR "${NCBI_PTBCFG_INSTALL_EXPORT} was not found in ${NCBI_EXTERNAL_BUILD_ROOT}/${NCBI_DIRNAME_EXPORT}")
    endif()
endif()

#set(CMAKE_MODULE_PATH "${NCBI_SRC_ROOT}/build-system/cmake/" ${CMAKE_MODULE_PATH})
if(EXISTS "${NCBI_TREE_CMAKECFG}/modules")
    list(APPEND CMAKE_MODULE_PATH "${NCBI_TREE_CMAKECFG}/modules")
endif()

if(NOT NCBI_PTBCFG_COLLECT_REQUIRES)
#############################################################################
# Basic checks
include(${NCBI_TREE_CMAKECFG}/CMakeChecks.basic-checks.cmake)

#############################################################################
# External libraries
include(${NCBI_TREE_CMAKECFG}/CMake.NCBIComponents.cmake)

#############################################################################
# Generation of configuration files

include(${NCBI_TREE_CMAKECFG}/CMakeChecks.srcid.cmake)

cmake_host_system_information(RESULT _local_host_name  QUERY HOSTNAME)
if (WIN32 OR XCODE)
    set(HOST "${HOST_CPU}-${HOST_OS}")
else()
#    set(HOST "${HOST_CPU}-unknown-${HOST_OS}")
    set(HOST "${HOST_CPU}-${HOST_OS}")
endif()
set(FEATURES ${NCBI_ALL_COMPONENTS};${NCBI_ALL_REQUIRES})
string(REPLACE ";" " " FEATURES "${FEATURES}")

set(_tk_common_include "${NCBITK_INC_ROOT}/common")
if (WIN32 OR XCODE)
    foreach(_cfg ${NCBI_CONFIGURATION_TYPES})

        set(_file "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${_cfg}")
        if (WIN32)
            string(REPLACE "/" "\\\\" _file ${_file})
        endif()
        set(c_ncbi_runpath "${_file}")
        if (WIN32)
            string(REPLACE "/" "\\\\" SYBASE_PATH "${SYBASE_PATH}")
        endif()

        set(NCBI_SIGNATURE "${NCBI_COMPILER}_${NCBI_COMPILER_VERSION}-${_cfg}--${HOST_CPU}-${HOST_OS_WITH_VERSION}-${_local_host_name}")
        set(NCBI_SIGNATURE_CFG "${NCBI_COMPILER}_${NCBI_COMPILER_VERSION}-\$<CONFIG>--${HOST_CPU}-${HOST_OS_WITH_VERSION}-${_local_host_name}")
        set(NCBI_SIGNATURE_${_cfg} "${NCBI_SIGNATURE}")
if(OFF)
        if (WIN32)
            configure_file(${NCBI_TREE_CMAKECFG}/ncbiconf_msvc_site.h.in ${NCBI_CFGINC_ROOT}/${_cfg}/common/config/ncbiconf_msvc_site.h)
        elseif (XCODE)
            configure_file(${NCBI_TREE_CMAKECFG}/ncbiconf_msvc_site.h.in ${NCBI_CFGINC_ROOT}/${_cfg}/common/config/ncbiconf_xcode_site.h)
        endif()
else()
        if (WIN32)
            configure_file(${NCBI_TREE_CMAKECFG}/config.cmake.h.in ${NCBI_CFGINC_ROOT}/${_cfg}/common/config/ncbiconf_msvc.h)
        elseif (XCODE)
            configure_file(${NCBI_TREE_CMAKECFG}/config.cmake.h.in ${NCBI_CFGINC_ROOT}/${_cfg}/common/config/ncbiconf_xcode.h)
        endif()
endif()
        if (EXISTS ${NCBI_SRC_ROOT}/corelib/ncbicfg.c.in)
            configure_file(${NCBI_SRC_ROOT}/corelib/ncbicfg.c.in ${NCBI_CFGINC_ROOT}/${_cfg}/common/config/ncbicfg.cfg.c)
        elseif (EXISTS ${NCBITK_SRC_ROOT}/corelib/ncbicfg.c.in)
            configure_file(${NCBITK_SRC_ROOT}/corelib/ncbicfg.c.in ${NCBI_CFGINC_ROOT}/${_cfg}/common/config/ncbicfg.cfg.c)
        endif()
        if(EXISTS "${_tk_common_include}/ncbi_build_ver.h.in")
            configure_file(${_tk_common_include}/ncbi_build_ver.h.in ${NCBI_CFGINC_ROOT}/${_cfg}/common/ncbi_build_ver.h)
        endif()
    endforeach()
    if(NOT EXISTS ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}/corelib/ncbicfg.c)
        file(WRITE ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}/corelib/ncbicfg.c "#include <common/config/ncbicfg.cfg.c>\n")
    endif()
    if (WIN32)
        if (BUILD_SHARED_LIBS)
            set(NCBITEST_SIGNATURE "${NCBI_COMPILER_ALT}-\$<CONFIG>MTdll64--${HOST_CPU}-win64-${_local_host_name}")
        else()
            set(NCBITEST_SIGNATURE "${NCBI_COMPILER_ALT}-\$<CONFIG>MTstatic64--${HOST_CPU}-win64-${_local_host_name}")
        endif()
    elseif (XCODE)
        if (BUILD_SHARED_LIBS)
            set(NCBITEST_SIGNATURE "${NCBI_COMPILER}_${NCBI_COMPILER_VERSION}-\$<CONFIG>MTdll64--${HOST_CPU}-${HOST_OS_WITH_VERSION}-${_local_host_name}")
        else()
            set(NCBITEST_SIGNATURE "${NCBI_COMPILER}_${NCBI_COMPILER_VERSION}-\$<CONFIG>MTstatic64--${HOST_CPU}-${HOST_OS_WITH_VERSION}-${_local_host_name}")
        endif()
    endif()
else()
#Linux
    set(c_ncbi_runpath ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
    set(NCBI_TLS_VAR "__thread")

    set(NCBI_SIGNATURE "${NCBI_COMPILER}_${NCBI_COMPILER_VERSION}-${NCBI_BUILD_TYPE}--${HOST_CPU}-${HOST_OS_WITH_VERSION}-${_local_host_name}")
    if (BUILD_SHARED_LIBS)
        set(NCBITEST_SIGNATURE "${NCBI_COMPILER}_${NCBI_COMPILER_VERSION}-${CMAKE_BUILD_TYPE}MTdll64--${HOST_CPU}-${HOST_OS_WITH_VERSION}-${_local_host_name}")
    else()
        set(NCBITEST_SIGNATURE "${NCBI_COMPILER}_${NCBI_COMPILER_VERSION}-${CMAKE_BUILD_TYPE}MTstatic64--${HOST_CPU}-${HOST_OS_WITH_VERSION}-${_local_host_name}")
    endif()
    configure_file(${NCBI_TREE_CMAKECFG}/config.cmake.h.in ${NCBI_CFGINC_ROOT}/ncbiconf_unix.h)
    if (EXISTS ${NCBI_SRC_ROOT}/corelib/ncbicfg.c.in)
        configure_file(${NCBI_SRC_ROOT}/corelib/ncbicfg.c.in ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}/corelib/ncbicfg.c)
    elseif (EXISTS ${NCBITK_SRC_ROOT}/corelib/ncbicfg.c.in)
        configure_file(${NCBITK_SRC_ROOT}/corelib/ncbicfg.c.in ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}/corelib/ncbicfg.c)
    endif()

    if(EXISTS "${_tk_common_include}/ncbi_build_ver.h.in")
        configure_file(${_tk_common_include}/ncbi_build_ver.h.in ${NCBI_CFGINC_ROOT}/common/ncbi_build_ver.h)
    endif()
endif()

if(NOT DEFINED NCBI_EXTERNAL_TREE_ROOT AND "${NCBI_TREE_ROOT}" STREQUAL "${NCBITK_TREE_ROOT}")
    get_filename_component(_dir ${NCBI_BUILD_ROOT} DIRECTORY)
    if("${_dir}" STREQUAL "${NCBI_TREE_ROOT}")
        NCBI_util_gitignore(${NCBI_BUILD_ROOT})
    endif()
endif()

endif(NOT NCBI_PTBCFG_COLLECT_REQUIRES)
