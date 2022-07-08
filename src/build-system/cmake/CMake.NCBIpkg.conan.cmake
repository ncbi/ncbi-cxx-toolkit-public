#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI C++ Toolkit Conan package adapter
##  it is used when the Toolkit is built and installed as Conan package
##    Author: Andrei Gourianov, gouriano@ncbi
##

if(NOT DEFINED NCBI_TOOLKIT_NCBIPTB_BUILD_SYSTEM_INCLUDED)
set( NCBI_TOOLKIT_NCBIPTB_BUILD_SYSTEM_INCLUDED ON)

###############################################################################
cmake_policy(SET CMP0054 NEW)
cmake_policy(SET CMP0057 NEW)

if(NOT DEFINED NCBI_PTBCFG_ENABLE_TOOLS)
    set(NCBI_PTBCFG_ENABLE_TOOLS             ON)
endif()
set(NCBI_PTBCFG_PACKAGED               ON)
set(NCBI_PTBCFG_ENABLE_COLLECTOR       ON)
#set(NCBI_VERBOSE_ALLPROJECTS           OFF)
#set(NCBI_PTBCFG_ALLOW_COMPOSITE        OFF)
#set(NCBI_PTBCFG_ADDTEST                OFF)

###############################################################################
set(NCBI_PTBCFG_INSTALL_EXPORT ncbi-cpp-toolkit)

set(_listdir "${CMAKE_CURRENT_LIST_DIR}")
if(NCBI_PTBCFG_ENABLE_TOOLS)
    include(${_listdir}/CMake.NCBIptb.definitions.cmake)
    include(${_listdir}/CMakeMacros.cmake)
    include(${_listdir}/CMake.NCBIptb.cmake)
    include(${_listdir}/CMakeChecks.compiler.cmake)
    include(${_listdir}/CMake.NCBIComponents.cmake)
    include_directories(${NCBITK_INC_ROOT} ${NCBI_INC_ROOT})

    include(${_listdir}/CMake.NCBIptb.datatool.cmake)
    include(${_listdir}/CMake.NCBIptb.grpc.cmake)
    include(${_listdir}/CMake.NCBIpkg.codegen.cmake)
    if(NCBI_PTBCFG_ADDTEST)
        include(${_listdir}/CMake.NCBIptb.ctest.cmake)
    endif()

    if (DEFINED NCBI_EXTERNAL_TREE_ROOT)
        set(NCBI_EXTERNAL_BUILD_ROOT ${NCBI_EXTERNAL_TREE_ROOT})
        if (NOT DEFINED NCBI_PTBCFG_NOIMPORT)
            if (EXISTS ${NCBI_EXTERNAL_BUILD_ROOT}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}.cmake)
                include(${NCBI_EXTERNAL_BUILD_ROOT}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}.cmake)
            else()
                message(FATAL_ERROR "${NCBI_PTBCFG_INSTALL_EXPORT} was not found in ${NCBI_EXTERNAL_BUILD_ROOT}/${NCBI_DIRNAME_EXPORT}")
            endif()
            NCBI_import_hostinfo(${NCBI_EXTERNAL_BUILD_ROOT}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}.hostinfo)
            NCBI_process_imports(${NCBI_EXTERNAL_BUILD_ROOT}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}.imports)
        endif()
    endif()

    include(${_listdir}/CMakeChecks.final-message.cmake)
else()
    include(${_listdir}/../../${NCBI_PTBCFG_INSTALL_EXPORT}.cmake)
    include(${_listdir}/CMake.NCBIpkg.codegen.cmake)
endif()
endif(NOT DEFINED NCBI_TOOLKIT_NCBIPTB_BUILD_SYSTEM_INCLUDED)
