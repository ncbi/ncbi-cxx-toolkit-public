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
cmake_policy(SET CMP0074 NEW)
set(NCBI_ALL_COMPONENTS "")
set(NCBI_ALL_REQUIRES "")
set(NCBI_ALL_DISABLED "")
set(NCBI_ALL_LEGACY "")
set(NCBI_ALL_DISABLED_LEGACY "")

#############################################################################
macro(NCBIcomponent_report _name)
    if (NCBI_COMPONENT_${_name}_DISABLED)
        NCBI_notice("DISABLED ${_name}")
    endif()
    if (NOT ${_name} IN_LIST NCBI_ALL_COMPONENTS AND
        NOT ${_name} IN_LIST NCBI_ALL_REQUIRES AND
        NOT ${_name} IN_LIST NCBI_ALL_DISABLED)
        if(NOT DEFINED NCBI_COMPONENT_${_name}_FOUND AND NOT DEFINED NCBI_REQUIRE_${_name}_FOUND)
            set(NCBI_REQUIRE_${_name}_FOUND NO)
        endif()
        if(NCBI_COMPONENT_${_name}_FOUND)
            list(APPEND NCBI_ALL_COMPONENTS ${_name})
        elseif(NCBI_REQUIRE_${_name}_FOUND)
            list(APPEND NCBI_ALL_REQUIRES ${_name})
        else()
            list(APPEND NCBI_ALL_DISABLED ${_name})
        endif()
    endif()
endmacro()
macro(NCBIcomponent_deprecated_name _deprecated_name _newname)
    if(NCBI_COMPONENT_${_newname}_FOUND)
        list(APPEND NCBI_ALL_LEGACY ${_deprecated_name})
        set(NCBI_COMPONENT_${_deprecated_name}_FOUND ${_newname})
    else()
        list(APPEND NCBI_ALL_DISABLED_LEGACY ${_deprecated_name})
    endif()
endmacro()
#############################################################################

set(NCBI_REQUIRE_MT_FOUND YES)
list(APPEND NCBI_ALL_REQUIRES MT)
set(NCBI_PlatformBits 64)
set(NCBI_TOOLS_ROOT $ENV{NCBI})

if(BUILD_SHARED_LIBS)
    set(NCBI_REQUIRE_DLL_BUILD_FOUND YES)
    set(NCBI_REQUIRE_DLL_FOUND YES)
endif()
NCBIcomponent_report(DLL_BUILD)
NCBIcomponent_report(DLL)

if(UNIX)
    set(NCBI_REQUIRE_unix_FOUND YES)
    NCBIcomponent_report(unix)
    if(APPLE)
        set(NCBI_REQUIRE_MacOS_FOUND YES)
        NCBIcomponent_report(MacOS)
    elseif(CYGWIN)
        set(NCBI_REQUIRE_Cygwin_FOUND YES)
        NCBIcomponent_report(Cygwin)
    else()
        if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
            set(NCBI_REQUIRE_Linux_FOUND YES)
            NCBIcomponent_report(Linux)
        elseif(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
            set(NCBI_REQUIRE_FreeBSD_FOUND YES)
            NCBIcomponent_report(FreeBSD)
        endif()
    endif()
elseif(WIN32)
    set(NCBI_REQUIRE_MSWin_FOUND YES)
    string(REPLACE "\\" "/" NCBI_TOOLS_ROOT "${NCBI_TOOLS_ROOT}")
    NCBIcomponent_report(MSWin)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

set(NCBI_REQUIRE_${NCBI_COMPILER}_FOUND YES)
list(APPEND NCBI_ALL_REQUIRES ${NCBI_COMPILER})

if(NOT "${NCBI_PTBCFG_PROJECT_COMPONENTS}" STREQUAL "")
    string(REPLACE "," ";" NCBI_PTBCFG_PROJECT_COMPONENTS "${NCBI_PTBCFG_PROJECT_COMPONENTS}")
    string(REPLACE " " ";" NCBI_PTBCFG_PROJECT_COMPONENTS "${NCBI_PTBCFG_PROJECT_COMPONENTS}")
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
foreach( _comp IN ITEMS GNUTLS WGMLST)
    if(NOT DEFINED NCBI_COMPONENT_${_comp}_DISABLED)
        set(NCBI_COMPONENT_${_comp}_DISABLED YES)
    endif()
endforeach()

#############################################################################
include(${NCBI_TREE_CMAKECFG}/CMake.NCBIComponentsCheck.cmake)

#############################################################################
# ORIG_LIBS
if(UNIX)
    include(CheckLibraryExists)

    NCBI_find_system_library(DL_LIBS dl)
    if(DL_LIBS)
        set(HAVE_LIBDL 1)
    endif()
    set(THREAD_LIBS   ${CMAKE_THREAD_LIBS_INIT})
    NCBI_find_system_library(CRYPT_LIBS crypt)
    NCBI_find_system_library(MATH_LIBS m)

    if (APPLE)
        NCBI_find_system_library(NETWORK_LIBS resolv)
        NCBI_find_system_library(RT_LIBS c)
    elseif (NCBI_REQUIRE_FreeBSD_FOUND)
        NCBI_find_system_library(NETWORK_LIBS c)
        NCBI_find_system_library(RT_LIBS      rt)
    else ()
        NCBI_find_system_library(NETWORK_LIBS   resolv)
        if(NCBI_PTBCFG_COMPONENT_StaticComponents)
            NCBI_find_system_library(RT_LIBS        rt STATIC)
        else()
            NCBI_find_system_library(RT_LIBS        rt)
        endif()
    endif ()
    set(ORIG_LIBS   ${DL_LIBS} ${RT_LIBS} ${MATH_LIBS} ${CMAKE_THREAD_LIBS_INIT})

    if(NOT HAVE_LIBICONV)
        NCBI_find_system_library(ICONV_LIBS    iconv)
        if(ICONV_LIBS)
            set(HAVE_LIBICONV 1)
        endif()
    endif()
    if(HAVE_LIBICONV)
        set(NCBI_REQUIRE_Iconv_FOUND YES)
        list(APPEND NCBI_ALL_REQUIRES Iconv)
    endif()

    try_compile(NCBI_ATOMIC64_STD ${CMAKE_BINARY_DIR}
                ${CMAKE_CURRENT_LIST_DIR}/test-atomic64.cpp)
    if(NCBI_ATOMIC64_STD)
        message(STATUS "Found 64-bit atomic<> support in standard libraries")
        set(ATOMIC64_LIBS "")
    else()
        NCBI_find_system_library(ATOMIC64_LIBS atomic)
        try_compile(NCBI_ATOMIC64_LIBATOMIC ${CMAKE_BINARY_DIR}
                    ${CMAKE_CURRENT_LIST_DIR}/test-atomic64.cpp
                    LINK_LIBRARIES ${ATOMIC64_LIBS})
        if(NCBI_ATOMIC64_LIBATOMIC)
            message(STATUS "Found 64-bit atomic<> support in ${ATOMIC64_LIBS}")
        else()
            message(WARNING "64-bit atomic<> support not found")
        endif()
    endif()
elseif(WIN32)
    set(ORIG_LIBS bcrypt.lib ws2_32.lib dbghelp.lib)
endif()
if(__NCBI_PTBCFG_ORIGLIBS)
  return()
endif()
#############################################################################
# TLS
set(NCBI_REQUIRE_TLS_FOUND YES)
list(APPEND NCBI_ALL_REQUIRES TLS)

#############################################################################
# LocalPCRE
if (NOT NCBI_COMPONENT_LocalPCRE_DISABLED AND EXISTS ${NCBITK_INC_ROOT}/util/regexp AND NOT NCBI_PTBCFG_PACKAGING)
    set(NCBI_COMPONENT_LocalPCRE_FOUND LOCAL)
    set(NCBI_COMPONENT_LocalPCRE_INCLUDE ${NCBITK_INC_ROOT}/util/regexp)
    set(NCBI_COMPONENT_LocalPCRE_NCBILIB regexp)
endif()
NCBIcomponent_report(LocalPCRE)

#############################################################################
# LocalZ
if (NOT NCBI_COMPONENT_LocalZ_DISABLED AND EXISTS ${NCBITK_INC_ROOT}/util/compress/zlib AND NOT NCBI_PTBCFG_PACKAGING)
    set(NCBI_COMPONENT_LocalZ_FOUND LOCAL)
    set(NCBI_COMPONENT_LocalZ_INCLUDE ${NCBITK_INC_ROOT}/util/compress/zlib)
    set(NCBI_COMPONENT_LocalZ_NCBILIB z)
endif()
NCBIcomponent_report(LocalZ)

#############################################################################
# LocalZCF
if (NOT NCBI_COMPONENT_LocalZCF_DISABLED AND EXISTS ${NCBITK_INC_ROOT}/util/compress/zlib_cloudflare)
    set(NCBI_COMPONENT_LocalZCF_FOUND LOCAL)
    set(NCBI_COMPONENT_LocalZCF_INCLUDE ${NCBITK_INC_ROOT}/util/compress/zlib_cloudflare)
    set(NCBI_COMPONENT_LocalZCF_NCBILIB zcf)
endif()
set(HAVE_LIBZCF ${NCBI_COMPONENT_LocalZCF_FOUND})
NCBIcomponent_report(LocalZCF)

#############################################################################
# LocalBZ2
if (NOT NCBI_COMPONENT_LocalBZ2_DISABLED AND EXISTS ${NCBITK_INC_ROOT}/util/compress/bzip2 AND NOT NCBI_PTBCFG_PACKAGING)
    set(NCBI_COMPONENT_LocalBZ2_FOUND LOCAL)
    set(NCBI_COMPONENT_LocalBZ2_INCLUDE ${NCBITK_INC_ROOT}/util/compress/bzip2)
    set(NCBI_COMPONENT_LocalBZ2_NCBILIB bz2)
endif()
NCBIcomponent_report(LocalBZ2)

#############################################################################
# LocalLMDB
if (NOT NCBI_COMPONENT_LocalLMDB_DISABLED AND EXISTS ${NCBITK_INC_ROOT}/util/lmdb AND NOT CYGWIN AND NOT NCBI_PTBCFG_PACKAGING)
    set(NCBI_COMPONENT_LocalLMDB_FOUND LOCAL)
    set(NCBI_COMPONENT_LocalLMDB_INCLUDE ${NCBITK_INC_ROOT}/util/lmdb)
    set(NCBI_COMPONENT_LocalLMDB_NCBILIB lmdb)
endif()
NCBIcomponent_report(LocalLMDB)

#############################################################################
# connext
set(NCBI_REQUIRE_connext_FOUND YES)
set(HAVE_LIBCONNEXT 1)
NCBIcomponent_report(connext)

#############################################################################
# PubSeqOS
if (NOT NCBI_COMPONENT_PubSeqOS_DISABLED AND EXISTS ${NCBITK_SRC_ROOT}/objtools/data_loaders/genbank/pubseq/CMakeLists.txt)
    set(NCBI_REQUIRE_PubSeqOS_FOUND YES)
    set(HAVE_PUBSEQ_OS 1)
endif()
NCBIcomponent_report(PubSeqOS)

#############################################################################
# FreeTDS
if(NOT NCBI_COMPONENT_FreeTDS_DISABLED
        AND EXISTS ${NCBITK_INC_ROOT}/dbapi/driver/ftds14
        AND EXISTS ${NCBITK_INC_ROOT}/dbapi/driver/ftds14/freetds)
    set(NCBI_COMPONENT_FreeTDS_FOUND   YES)
    set(HAVE_LIBFTDS 1)
    set(FTDS14_INCLUDE ${NCBITK_INC_ROOT}/dbapi/driver/ftds14 ${NCBITK_INC_ROOT}/dbapi/driver/ftds14/freetds)
    set(NCBI_COMPONENT_FreeTDS_INCLUDE ${FTDS14_INCLUDE})
endif()
NCBIcomponent_report(FreeTDS)

#############################################################################
set(NCBI_COMPONENT_Boost.Test.Included_NCBILIB test_boost)
set(NCBI_COMPONENT_SQLITE3_NCBILIB sqlitewrapp)
set(NCBI_COMPONENT_Sybase_NCBILIB  ncbi_xdbapi_ctlib)
set(NCBI_COMPONENT_ODBC_NCBILIB    ncbi_xdbapi_odbc)
set(NCBI_COMPONENT_FreeTDS_NCBILIB ct_ftds14 ncbi_xdbapi_ftds)
set(NCBI_COMPONENT_connext_NCBILIB xconnext)

#############################################################################
if(NCBI_PTBCFG_PACKAGING OR NCBI_PTBCFG_PACKAGED OR NCBI_PTBCFG_USECONAN OR NCBI_PTBCFG_HASCONAN)
    include(${NCBI_TREE_CMAKECFG}/CMake.NCBIComponentsPackage.cmake)
endif()
if(NOT DEFINED NCBI_PTBCFG_USELOCAL OR NCBI_PTBCFG_USELOCAL)
    if (MSVC)
        include(${NCBI_TREE_CMAKECFG}/CMake.NCBIComponentsMSVC.cmake)
    elseif (APPLE)
        include(${NCBI_TREE_CMAKECFG}/CMake.NCBIComponentsXCODE.cmake)
    else()
        include(${NCBI_TREE_CMAKECFG}/CMake.NCBIComponentsUNIXex.cmake)
    endif()
endif()
if(EXISTS ${NCBI_TREE_ROOT}/CMake.CustomComponents.cmake)
    include(${NCBI_TREE_ROOT}/CMake.CustomComponents.cmake)
endif()

#############################################################################
# NCBI_PROTOC_APP_VERSION
if(EXISTS "${NCBI_PROTOC_APP}")
    execute_process(
        COMMAND ${NCBI_PROTOC_APP} --version
        OUTPUT_VARIABLE NCBI_PROTOC_APP_VERSION
    )
endif()

#############################################################################
# NCBI_PYTHON_EXECUTABLE
if(NOT NCBI_PYTHON_EXECUTABLE)
    if(NOT "$ENV{VIRTUAL_ENV}" STREQUAL "")
        find_program(NCBI_PYTHON_EXECUTABLE python3 python NO_CACHE)
    endif()
    if(NOT NCBI_PYTHON_EXECUTABLE AND NCBI_COMPONENT_PYTHON_FOUND)
        if(DEFINED Python3_EXECUTABLE)
            set(NCBI_PYTHON_EXECUTABLE ${Python3_EXECUTABLE})
        elseif(DEFINED PYTHON_EXECUTABLE)
            set(NCBI_PYTHON_EXECUTABLE ${PYTHON_EXECUTABLE})
        elseif(EXISTS ${NCBI_ThirdParty_PYTHON}/bin/python3${CMAKE_EXECUTABLE_SUFFIX})
            set(NCBI_PYTHON_EXECUTABLE ${NCBI_ThirdParty_PYTHON}/bin/python3${CMAKE_EXECUTABLE_SUFFIX})
        elseif(EXISTS ${NCBI_ThirdParty_PYTHON}/bin/python${CMAKE_EXECUTABLE_SUFFIX})
            set(NCBI_PYTHON_EXECUTABLE ${NCBI_ThirdParty_PYTHON}/bin/python${CMAKE_EXECUTABLE_SUFFIX})
        elseif(EXISTS ${NCBI_ThirdParty_PYTHON}/python3${CMAKE_EXECUTABLE_SUFFIX})
            set(NCBI_PYTHON_EXECUTABLE ${NCBI_ThirdParty_PYTHON}/python3${CMAKE_EXECUTABLE_SUFFIX})
        elseif(EXISTS ${NCBI_ThirdParty_PYTHON}/python${CMAKE_EXECUTABLE_SUFFIX})
            set(NCBI_PYTHON_EXECUTABLE ${NCBI_ThirdParty_PYTHON}/python${CMAKE_EXECUTABLE_SUFFIX})
        endif()
    endif()
endif()
if(NOT NCBI_PYTHON_EXECUTABLE)
    find_package(Python3)
    if (Python3_Interpreter_FOUND)
        set(NCBI_PYTHON_EXECUTABLE ${Python3_EXECUTABLE})
    endif()
endif()

#############################################################################
# FreeTDS
set(FTDS100_INCLUDE ${NCBITK_INC_ROOT}/dbapi/driver/ftds100 ${NCBITK_INC_ROOT}/dbapi/driver/ftds100/freetds)
set(FTDS14_INCLUDE ${NCBITK_INC_ROOT}/dbapi/driver/ftds14 ${NCBITK_INC_ROOT}/dbapi/driver/ftds14/freetds)

#############################################################################
# NCBILS2
if (NOT NCBI_COMPONENT_NCBILS2_DISABLED AND NCBI_COMPONENT_GCRYPT_FOUND AND EXISTS ${NCBITK_SRC_ROOT}/internal/ncbils2/CMakeLists.txt)
    set(NCBI_REQUIRE_NCBILS2_FOUND YES)
endif()
NCBIcomponent_report(NCBILS2)

#############################################################################
# PSGLoader
if(NOT NCBI_COMPONENT_PSGLoader_DISABLED AND NCBI_COMPONENT_UV_FOUND AND NCBI_COMPONENT_NGHTTP2_FOUND)
    set(HAVE_PSG_LOADER 1)
    set(NCBI_REQUIRE_PSGLoader_FOUND YES)
endif()
NCBIcomponent_report(PSGLoader)

#############################################################################
# Sybase

if (UNIX AND NOT APPLE AND NOT NCBI_COMPONENT_Sybase_FOUND)
    if (NOT NCBI_COMPONENT_Sybase_DISABLED)
        set(NCBI_ThirdParty_SybaseNetPath "/opt/sybase/clients/16.0-64bit" CACHE PATH "SybaseNetPath")
        set(NCBI_ThirdParty_SybaseLocalPath "" CACHE PATH "SybaseLocalPath")
        file(GLOB _files LIST_DIRECTORIES TRUE "${NCBI_ThirdParty_SybaseNetPath}/OCS*")
        if(_files)
            list(GET _files 0 NCBI_ThirdParty_Sybase)
        endif()
        #NCBI_define_component(Sybase sybdb64 sybblk_r64 sybct_r64 sybcs_r64 sybtcl_r64 sybcomn_r64 sybintl_r64 sybunic64)
        set(__StaticComponents ${NCBI_PTBCFG_COMPONENT_StaticComponents})
        set(NCBI_PTBCFG_COMPONENT_StaticComponents FALSE)
        NCBI_define_Xcomponent(NAME Sybase  LIB sybblk_r64 sybct_r64 sybcs_r64 sybtcl_r64 sybcomn_r64 sybintl_r64 sybunic64)
        set(NCBI_PTBCFG_COMPONENT_StaticComponents ${__StaticComponents})
    endif()
    NCBIcomponent_report(Sybase)
    if (NCBI_COMPONENT_Sybase_FOUND)
        set(NCBI_COMPONENT_Sybase_DEFINES ${NCBI_COMPONENT_Sybase_DEFINES} SYB_LP64)
        set(SYBASE_PATH ${NCBI_ThirdParty_SybaseNetPath})
        set(SYBASE_LCL_PATH "${NCBI_ThirdParty_SybaseLocalPath}")
        if(CMAKE_COMPILER_IS_GNUCC)
            set(NCBI_COMPONENT_Sybase_LIBS -Wl,--no-as-needed ${NCBI_COMPONENT_Sybase_LIBS})
        endif()
    endif()
endif()

#############################################################################
NCBIcomponent_deprecated_name(C-Toolkit NCBI_C)
NCBIcomponent_deprecated_name(Fast-CGI FASTCGI)
NCBIcomponent_deprecated_name(LIBXML XML)
NCBIcomponent_deprecated_name(LIBXSLT XSLT)
NCBIcomponent_deprecated_name(LIBEXSLT EXSLT)
NCBIcomponent_deprecated_name(LIBXLSXWRITER XLSXWRITER)
NCBIcomponent_deprecated_name(MESA OSMesa)

NCBIcomponent_deprecated_name(Xalan XALAN)
NCBIcomponent_deprecated_name(Xerces XERCES)
NCBIcomponent_deprecated_name(LIBUV UV)
NCBIcomponent_deprecated_name(OPENSSL OpenSSL)
NCBIcomponent_deprecated_name(MONGODB3 MONGOCXX)

#############################################################################
list(SORT NCBI_ALL_LEGACY)
list(APPEND NCBI_ALL_COMPONENTS ${NCBI_ALL_LEGACY})
list(SORT NCBI_ALL_COMPONENTS)
list(SORT NCBI_ALL_REQUIRES)
list(SORT NCBI_ALL_DISABLED)

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
                if( NOT "${NCBI_COMPONENT_${_value}_FOUND}" STREQUAL "LOCAL" AND
                    NOT "${NCBI_REQUIRE_${_value}_FOUND}"   STREQUAL "LOCAL")
                    message(SEND_ERROR "Component ${_value} is enabled, but not allowed")
                endif()
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
