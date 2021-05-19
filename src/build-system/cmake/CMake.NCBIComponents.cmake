#############################################################################
# $Id$
#############################################################################

##
## NCBI CMake components description
##
##
## As a result, the following variables should be defined for component XXX
##  NCBI_COMPONENT_XXX_FOUND
##  NCBI_COMPONENT_XXX_INCLUDE
##  NCBI_COMPONENT_XXX_DEFINES
##  NCBI_COMPONENT_XXX_LIBS
##  HAVE_LIBXXX


#############################################################################
set(NCBI_ALL_COMPONENTS "")
set(NCBI_ALL_LEGACY "")
set(NCBI_ALL_REQUIRES "")

set(NCBI_REQUIRE_MT_FOUND YES)
list(APPEND NCBI_ALL_REQUIRES MT)
set(NCBI_PlatformBits 64)
set(NCBI_TOOLS_ROOT $ENV{NCBI})

if(BUILD_SHARED_LIBS)
    set(NCBI_REQUIRE_DLL_BUILD_FOUND YES)
    list(APPEND NCBI_ALL_REQUIRES DLL_BUILD)
endif()

if(UNIX)
    set(NCBI_REQUIRE_unix_FOUND YES)
    list(APPEND NCBI_ALL_REQUIRES unix)
    if(NOT APPLE AND NOT CYGWIN)
        if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
            set(NCBI_REQUIRE_Linux_FOUND YES)
            list(APPEND NCBI_ALL_REQUIRES Linux)
        elseif(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
            set(NCBI_REQUIRE_FreeBSD_FOUND YES)
            list(APPEND NCBI_ALL_REQUIRES FreeBSD)
        endif()
    endif()
elseif(WIN32)
    set(NCBI_REQUIRE_MSWin_FOUND YES)
    list(APPEND NCBI_ALL_REQUIRES MSWin)
    if(BUILD_SHARED_LIBS)
        set(NCBI_REQUIRE_DLL_FOUND YES)
        list(APPEND NCBI_ALL_REQUIRES DLL)
    endif()
    string(REPLACE "\\" "/" NCBI_TOOLS_ROOT "${NCBI_TOOLS_ROOT}")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

set(NCBI_REQUIRE_${NCBI_COMPILER}_FOUND YES)
list(APPEND NCBI_ALL_REQUIRES ${NCBI_COMPILER})

if(NOT "${NCBI_PTBCFG_PROJECT_COMPONENTS}" STREQUAL "")
    string(REPLACE "," ";" NCBI_PTBCFG_PROJECT_COMPONENTS "${NCBI_PTBCFG_PROJECT_COMPONENTS}")
    foreach(_comp IN LISTS NCBI_PTBCFG_PROJECT_COMPONENTS)
        if("${_comp}" STREQUAL "")
            continue()
        endif()
        string(SUBSTRING ${_comp} 0 1 _sign)
        if ("${_sign}" STREQUAL "-")
            string(SUBSTRING ${_comp} 1 -1 _comp)
            set(NCBI_COMPONENT_${_comp}_DISABLED YES)
        else()
            set(NCBI_COMPONENT_${_comp}_DISABLED NO)
        endif()
    endforeach()
endif()

#############################################################################
include(${NCBI_TREE_CMAKECFG}/CMake.NCBIComponentsCheck.cmake)

#############################################################################
# ORIG_LIBS
if(UNIX)
    include(CheckLibraryExists)
    include(${NCBI_TREE_CMAKECFG}/FindExternalLibrary.cmake)

    check_library_exists(dl dlopen "" HAVE_LIBDL)
    if(HAVE_LIBDL)
        set(DL_LIBS -ldl)
    else(HAVE_LIBDL)
        message(FATAL_ERROR "dl library not found")
    endif(HAVE_LIBDL)

    set(THREAD_LIBS   ${CMAKE_THREAD_LIBS_INIT})
    find_library(CRYPT_LIBS NAMES crypt)
    find_library(MATH_LIBS NAMES m)

    if (APPLE)
        find_library(NETWORK_LIBS resolv)
        find_library(RT_LIBS c)
    elseif (NCBI_REQUIRE_FreeBSD_FOUND)
        find_library(NETWORK_LIBS c)
        find_library(RT_LIBS      rt)
    else ()
        find_library(NETWORK_LIBS   resolv)
        find_library(RT_LIBS        rt)
    endif ()
    set(ORIG_LIBS   ${DL_LIBS} ${RT_LIBS} ${MATH_LIBS} ${CMAKE_THREAD_LIBS_INIT})
elseif(WIN32)
    set(ORIG_LIBS ws2_32.lib dbghelp.lib)
endif()

#############################################################################
# local_lbsm
if(WIN32 OR CYGWIN OR NCBI_COMPONENT_local_lbsm_DISABLED)
    set(NCBI_COMPONENT_local_lbsm_FOUND NO)
else()
    if (EXISTS ${NCBITK_SRC_ROOT}/connect/ncbi_lbsm.c)
        set(NCBI_COMPONENT_local_lbsm_FOUND YES)
        list(APPEND NCBI_ALL_REQUIRES local_lbsm)
        set(HAVE_LOCAL_LBSM 1)
    else()
        set(NCBI_COMPONENT_local_lbsm_FOUND NO)
    endif()
endif()

#############################################################################
# LocalPCRE
if (EXISTS ${NCBITK_INC_ROOT}/util/regexp AND NOT NCBI_COMPONENT_LocalPCRE_DISABLED)
    set(NCBI_COMPONENT_LocalPCRE_FOUND YES)
    list(APPEND NCBI_ALL_REQUIRES LocalPCRE)
    set(NCBI_COMPONENT_LocalPCRE_INCLUDE ${NCBITK_INC_ROOT}/util/regexp)
    set(NCBI_COMPONENT_LocalPCRE_NCBILIB regexp)
else()
    set(NCBI_COMPONENT_LocalPCRE_FOUND NO)
endif()

#############################################################################
# LocalZ
if (EXISTS ${NCBITK_INC_ROOT}/util/compress/zlib AND NOT NCBI_COMPONENT_LocalZ_DISABLED)
    set(NCBI_COMPONENT_LocalZ_FOUND YES)
    list(APPEND NCBI_ALL_REQUIRES LocalZ)
    set(NCBI_COMPONENT_LocalZ_INCLUDE ${NCBITK_INC_ROOT}/util/compress/zlib)
    set(NCBI_COMPONENT_LocalZ_NCBILIB z)
else()
    set(NCBI_COMPONENT_LocalZ_FOUND NO)
endif()

#############################################################################
# LocalBZ2
if (EXISTS ${NCBITK_INC_ROOT}/util/compress/bzip2 AND NOT NCBI_COMPONENT_LocalBZ2_DISABLED)
    set(NCBI_COMPONENT_LocalBZ2_FOUND YES)
    list(APPEND NCBI_ALL_REQUIRES LocalBZ2)
    set(NCBI_COMPONENT_LocalBZ2_INCLUDE ${NCBITK_INC_ROOT}/util/compress/bzip2)
    set(NCBI_COMPONENT_LocalBZ2_NCBILIB bz2)
else()
    set(NCBI_COMPONENT_LocalBZ2_FOUND NO)
endif()

#############################################################################
# LocalLMDB
if (EXISTS ${NCBITK_INC_ROOT}/util/lmdb AND NOT CYGWIN AND NOT NCBI_COMPONENT_LocalLMDB_DISABLED)
    set(NCBI_COMPONENT_LocalLMDB_FOUND YES)
    list(APPEND NCBI_ALL_REQUIRES LocalLMDB)
    set(NCBI_COMPONENT_LocalLMDB_INCLUDE ${NCBITK_INC_ROOT}/util/lmdb)
    set(NCBI_COMPONENT_LocalLMDB_NCBILIB lmdb)
else()
    set(NCBI_COMPONENT_LocalLMDB_FOUND NO)
endif()

#############################################################################
# connext
set(NCBI_REQUIRE_connext_FOUND NO)
if (EXISTS ${NCBITK_SRC_ROOT}/connect/ext/CMakeLists.txt AND NOT NCBI_COMPONENT_connext_DISABLED)
    set(NCBI_REQUIRE_connext_FOUND YES)
    set(HAVE_LIBCONNEXT 1)
    list(APPEND NCBI_ALL_REQUIRES connext)
endif()

#############################################################################
# PubSeqOS
set(NCBI_REQUIRE_PubSeqOS_FOUND NO)
if (EXISTS ${NCBITK_SRC_ROOT}/objtools/data_loaders/genbank/pubseq/CMakeLists.txt
    AND NOT NCBI_COMPONENT_PubSeqOS_DISABLED)
    set(NCBI_REQUIRE_PubSeqOS_FOUND YES)
    list(APPEND NCBI_ALL_REQUIRES PubSeqOS)
endif()

#############################################################################
# FreeTDS
set(FTDS100_INCLUDE ${NCBITK_INC_ROOT}/dbapi/driver/ftds100 ${NCBITK_INC_ROOT}/dbapi/driver/ftds100/freetds)

set(NCBI_COMPONENT_FreeTDS_FOUND   NO)
if(NOT NCBI_COMPONENT_FreeTDS_DISABLED)
    set(NCBI_COMPONENT_FreeTDS_FOUND   YES)
    set(HAVE_LIBFTDS 1)
    list(APPEND NCBI_ALL_REQUIRES FreeTDS)
    set(NCBI_COMPONENT_FreeTDS_INCLUDE ${FTDS100_INCLUDE})
#set(NCBI_COMPONENT_FreeTDS_LIBS    ct_ftds100)
endif()

#############################################################################
set(NCBI_COMPONENT_Boost.Test.Included_NCBILIB test_boost)
set(NCBI_COMPONENT_SQLITE3_NCBILIB sqlitewrapp)
set(NCBI_COMPONENT_Sybase_NCBILIB  ncbi_xdbapi_ctlib)
set(NCBI_COMPONENT_ODBC_NCBILIB    ncbi_xdbapi_odbc)
set(NCBI_COMPONENT_FreeTDS_NCBILIB ct_ftds100 ncbi_xdbapi_ftds)
set(NCBI_COMPONENT_connext_NCBILIB xconnext)

#############################################################################
if(NCBI_PTBCFG_PACKAGE)
    include(${NCBI_TREE_CMAKECFG}/CMake.NCBIComponentsPackage.cmake)
elseif (CONANCOMPONENTS)
    include(${NCBI_TREE_CMAKECFG}/CMake.NCBIComponentsConan.cmake)
elseif (MSVC)
    include(${NCBI_TREE_CMAKECFG}/CMake.NCBIComponentsMSVC.cmake)
elseif (APPLE)
    include(${NCBI_TREE_CMAKECFG}/CMake.NCBIComponentsXCODE.cmake)
else()
    if(ON)
        include(${NCBI_TREE_CMAKECFG}/CMake.NCBIComponentsUNIXex.cmake)
    else()
        include(${NCBI_TREE_CMAKECFG}/CMake.NCBIComponentsUNIX.cmake)
    endif()
endif()

#############################################################################
# FreeTDS
set(FTDS100_INCLUDE ${NCBITK_INC_ROOT}/dbapi/driver/ftds100 ${NCBITK_INC_ROOT}/dbapi/driver/ftds100/freetds)

#############################################################################
list(SORT NCBI_ALL_LEGACY)
list(APPEND NCBI_ALL_COMPONENTS ${NCBI_ALL_LEGACY})
list(SORT NCBI_ALL_COMPONENTS)
list(SORT NCBI_ALL_REQUIRES)

#############################################################################
# verify
if(NOT "${NCBI_PTBCFG_PROJECT_COMPONENTS}" STREQUAL "")
    foreach(_comp IN LISTS NCBI_PTBCFG_PROJECT_COMPONENTS)
        if("${_comp}" STREQUAL "")
            continue()
        endif()
        NCBI_util_parse_sign( ${_comp} _value _negate)
        if (_negate)
            if(NCBI_COMPONENT_${_value}_FOUND OR NCBI_REQUIRE_${_value}_FOUND)
                message(SEND_ERROR "Component ${_value} is enabled, but not allowed")
            elseif(NOT DEFINED NCBI_COMPONENT_${_value}_FOUND AND NOT DEFINED NCBI_REQUIRE_${_value}_FOUND)
                message(SEND_ERROR "Component ${_value} is unknown")
            endif()
        else()
            if(NOT NCBI_COMPONENT_${_value}_FOUND AND NOT NCBI_REQUIRE_${_value}_FOUND)
                message(SEND_ERROR "Required component ${_value} was not found")
            endif()
        endif()
    endforeach()
endif()
