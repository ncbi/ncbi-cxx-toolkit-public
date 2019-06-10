#############################################################################
# $Id$
#############################################################################

##
## NCBI CMake components description - UNIX/Linux
##
##
## As a result, the following variables should be defined for component XXX
##  NCBI_COMPONENT_XXX_FOUND
##  NCBI_COMPONENT_XXX_INCLUDE
##  NCBI_COMPONENT_XXX_DEFINES
##  NCBI_COMPONENT_XXX_LIBS
##  HAVE_LIBXXX
##  HAVE_XXX

set(NCBI_COMPONENT_unix_FOUND YES)
set(NCBI_COMPONENT_Linux_FOUND YES)
option(USE_LOCAL_BZLIB "Use a local copy of libbz2")
option(USE_LOCAL_PCRE "Use a local copy of libpcre")
#############################################################################
# common settings
set(NCBI_TOOLS_ROOT $ENV{NCBI})
set(NCBI_OPT_ROOT  /opt/ncbi/64)
set(NCBI_PlatformBits 64)

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
    find_library(NETWORK_LIBS c)
    find_library(RT_LIBS c)
else (APPLE)
    find_library(NETWORK_LIBS nsl)
    find_library(RT_LIBS        rt)
endif (APPLE)
set(ORIG_LIBS   ${DL_LIBS} ${RT_LIBS} ${MATH_LIBS} ${CMAKE_THREAD_LIBS_INIT})

############################################################################
# Kerberos 5 (via GSSAPI)
find_external_library(KRB5 INCLUDES gssapi/gssapi_krb5.h LIBS gssapi_krb5 krb5 k5crypto com_err)

#############################################################################
set(NCBI_ThirdParty_BACKWARD    ${NCBI_TOOLS_ROOT}/backward-cpp-1.3.20180206-44ae960)
set(NCBI_ThirdParty_UNWIND      ${NCBI_TOOLS_ROOT}/libunwind-1.1)
set(NCBI_ThirdParty_LMDB        ${NCBI_TOOLS_ROOT}/lmdb-0.9.21)
set(NCBI_ThirdParty_LZO         ${NCBI_TOOLS_ROOT}/lzo-2.05)
set(NCBI_ThirdParty_FASTCGI        ${NCBI_TOOLS_ROOT}/fcgi-2.4.0)
set(NCBI_ThirdParty_FASTCGI_SHLIB    ${NCBI_OPT_ROOT}/fcgi-2.4.0)
set(NCBI_ThirdParty_SQLITE3      ${NCBI_TOOLS_ROOT}/sqlite-3.26.0-ncbi1)
set(NCBI_ThirdParty_BerkeleyDB   ${NCBI_TOOLS_ROOT}/BerkeleyDB-4.6.21.1)
set(NCBI_ThirdParty_Sybase     "/opt/sybase/clients/16.0-64bit/OCS-16_0")
#set(NCBI_ThirdParty_VDB        "/opt/ncbi/64/trace_software/vdb/vdb-versions/cxx_toolkit/2")
set(NCBI_ThirdParty_VDB          ${NCBI_OPT_ROOT}/trace_software/vdb/vdb-versions/2.9.4-1)
set(NCBI_ThirdParty_XML          ${NCBI_TOOLS_ROOT}/libxml-2.7.8)
set(NCBI_ThirdParty_XSLT         ${NCBI_TOOLS_ROOT}/libxml-2.7.8)
set(NCBI_ThirdParty_EXSLT        ${NCBI_TOOLS_ROOT}/libxml-2.7.8)
set(NCBI_ThirdParty_XLSXWRITER   ${NCBI_TOOLS_ROOT}/libxlsxwriter-0.6.9)
set(NCBI_ThirdParty_FTGL         ${NCBI_TOOLS_ROOT}/ftgl-2.1.3-rc5)
set(NCBI_ThirdParty_GLEW         ${NCBI_TOOLS_ROOT}/glew-1.5.8)
set(NCBI_ThirdParty_OpenGL       ${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2)
set(NCBI_ThirdParty_OSMesa       ${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2)
set(NCBI_ThirdParty_XERCES       ${NCBI_TOOLS_ROOT}/xerces-3.1.2)
set(NCBI_ThirdParty_GRPC         ${NCBI_TOOLS_ROOT}/grpc-1.14.1-ncbi1)
set(NCBI_ThirdParty_PROTOBUF     ${NCBI_TOOLS_ROOT}/grpc-1.14.1-ncbi1)
set(NCBI_ThirdParty_XALAN        ${NCBI_TOOLS_ROOT}/xalan-1.11)
set(NCBI_ThirdParty_GPG          ${NCBI_TOOLS_ROOT}/libgpg-error-1.6)
set(NCBI_ThirdParty_GCRYPT       ${NCBI_TOOLS_ROOT}/libgcrypt-1.4.3)
set(NCBI_ThirdParty_MSGSL        ${NCBI_TOOLS_ROOT}/msgsl-0.0.20171114-1c95f94)
set(NCBI_ThirdParty_SGE          "/netmnt/gridengine/current")
set(NCBI_ThirdParty_LEVELDB      ${NCBI_TOOLS_ROOT}/leveldb-1.21)

#############################################################################
#############################################################################

function(NCBI_define_component _name)

    if(DEFINED NCBI_COMPONENT_${_name}_FOUND)
        message("Already defined ${_name}")
        return()
    endif()
# root
    set(_root "")
    if (DEFINED NCBI_ThirdParty_${_name})
        set(_root ${NCBI_ThirdParty_${_name}})
    else()
        string(FIND ${_name} "." dotfound)
        string(SUBSTRING ${_name} 0 ${dotfound} _dotname)
        if (DEFINED NCBI_ThirdParty_${_dotname})
            set(_root ${NCBI_ThirdParty_${_dotname}})
        else()
            message("NOT FOUND ${_name}: NCBI_ThirdParty_${_name} not found")
            return()
        endif()
    endif()

    if (EXISTS ${_root}/${NCBI_COMPILER}${NCBI_COMPILER_VERSION}-${NCBI_BUILD_TYPE}/include)
        set(NCBI_COMPONENT_${_name}_INCLUDE ${_root}/${NCBI_COMPILER}${NCBI_COMPILER_VERSION}-${NCBI_BUILD_TYPE}/include PARENT_SCOPE)
    elseif (EXISTS ${_root}/${NCBI_BUILD_TYPE}/include)
        set(NCBI_COMPONENT_${_name}_INCLUDE ${_root}/${NCBI_BUILD_TYPE}/include PARENT_SCOPE)
    elseif (EXISTS ${_root}/include)
        set(NCBI_COMPONENT_${_name}_INCLUDE ${_root}/include PARENT_SCOPE)
    else()
        message("NOT FOUND ${_name}: ${_root}/include not found")
        return()
    endif()

# libraries
    set(_args ${ARGN})
    if(BUILD_SHARED_LIBS)
        set(_suffixes .so .a)
    else()
        set(_suffixes .a .so)
    endif()
    set(_roots ${_root})
    set(_subdirs ${NCBI_COMPILER}${NCBI_COMPILER_VERSION}-${NCBI_BUILD_TYPE}/lib ${NCBI_BUILD_TYPE}/lib lib64 lib)
    if (BUILD_SHARED_LIBS AND DEFINED NCBI_ThirdParty_${_name}_SHLIB)
        set(_roots ${NCBI_ThirdParty_${_name}_SHLIB} ${_roots})
        set(_subdirs shlib64 shlib lib64 lib)
    endif()

    set(_all_found YES)
    set(_all_libs "")
    foreach(_root IN LISTS _roots)
        foreach(_libdir IN LISTS _subdirs)
            set(_all_found YES)
            set(_all_libs "")
            foreach(_lib IN LISTS _args)
                set(_this_found NO)
                foreach(_sfx IN LISTS _suffixes)
                    if(EXISTS ${_root}/${_libdir}/lib${_lib}${_sfx})
                        list(APPEND _all_libs ${_root}/${_libdir}/lib${_lib}${_sfx})
                        set(_this_found YES)
                        break()
                    endif()
                endforeach()
                if(NOT _this_found)
                    set(_all_found NO)
                    break()
                endif()
            endforeach()
            if(_all_found)
                break()
            endif()
        endforeach()
        if(_all_found)
            break()
        endif()
    endforeach()

    if(_all_found)
        message(STATUS "Found ${_name}: ${_root}")
        set(NCBI_COMPONENT_${_name}_FOUND YES PARENT_SCOPE)
#        set(NCBI_COMPONENT_${_name}_INCLUDE ${_root}/include)
        set(NCBI_COMPONENT_${_name}_LIBS ${_all_libs} PARENT_SCOPE)

        string(TOUPPER ${_name} _upname)
        set(HAVE_LIB${_upname} 1 PARENT_SCOPE)
        string(REPLACE "." "_" _altname ${_upname})
        set(HAVE_${_altname} 1 PARENT_SCOPE)

        list(APPEND NCBI_ALL_COMPONENTS ${_name})
        set(NCBI_ALL_COMPONENTS ${NCBI_ALL_COMPONENTS} PARENT_SCOPE)
    else()
        set(NCBI_COMPONENT_${_name}_FOUND NO)
        message("NOT FOUND ${_name}: some libraries not found at ${_root}")
    endif()

endfunction()

#############################################################################
macro(NCBI_find_library _name)
    if(NOT DEFINED NCBI_COMPONENT_${_name}_FOUND)
        set(_args ${ARGN})
        find_library(NCBI_COMPONENT_${_name}_LIBS ${_args})
        if(NCBI_COMPONENT_${_name}_LIBS)
            set(NCBI_COMPONENT_${_name}_FOUND YES)
            list(APPEND NCBI_ALL_COMPONENTS ${_name})
            message(STATUS "Found ${_name}: ${NCBI_COMPONENT_${_name}_LIBS}")

            string(TOUPPER ${_name} _upname)
            set(HAVE_LIB${_upname} 1)
            set(HAVE_${_upname} 1)
        else()
            set(NCBI_COMPONENT_${_name}_FOUND NO)
            message("NOT FOUND ${_name}")
        endif()
    else()
        message("Already defined ${_name}")
    endif()
endmacro()

#############################################################################
# NCBI_C
set(NCBI_C_ROOT "${NCBI_TOOLS_ROOT}/ncbi")
string(REGEX MATCH "NCBI_INT8_GI|NCBI_STRICT_GI" INT8GI_FOUND "${CMAKE_CXX_FLAGS}")
if (NOT "${INT8GI_FOUND}" STREQUAL "")
    if (EXISTS "${NCBI_C_ROOT}/ncbi.gi64")
        set(NCBI_CTOOLKIT_PATH "${NCBI_C_ROOT}/ncbi.gi64")
    elseif (EXISTS "${NCBI_C_ROOT}.gi64")
        set(NCBI_CTOOLKIT_PATH "${NCBI_C_ROOT}.gi64")
    else ()
        set(NCBI_CTOOLKIT_PATH "${NCBI_C_ROOT}")
    endif ()
else ()
    set(NCBI_CTOOLKIT_PATH "${NCBI_C_ROOT}")
endif ()

if (EXISTS "${NCBI_CTOOLKIT_PATH}/include64" AND EXISTS "${NCBI_CTOOLKIT_PATH}/lib64")
    set(NCBI_C_INCLUDE  "${NCBI_CTOOLKIT_PATH}/include64")
    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        set(NCBI_C_LIBPATH  "${NCBI_CTOOLKIT_PATH}/altlib64")
    else()
        set(NCBI_C_LIBPATH  "${NCBI_CTOOLKIT_PATH}/lib64")
    endif()

    set(NCBI_C_ncbi     "ncbi")
    if (APPLE)
        set(NCBI_C_ncbi ${NCBI_C_ncbi} -Wl,-framework,ApplicationServices)
    endif ()
    set(HAVE_NCBI_C YES)
else ()
    set(HAVE_NCBI_C NO)
endif ()

if(HAVE_NCBI_C)
    message("NCBI_C found at ${NCBI_C_INCLUDE}")
    set(NCBI_COMPONENT_NCBI_C_FOUND YES)
    set(NCBI_COMPONENT_NCBI_C_INCLUDE ${NCBI_C_INCLUDE})
    set(_c_libs  ncbiobj ncbimmdb ${NCBI_C_ncbi})
    set(NCBI_COMPONENT_NCBI_C_LIBS -L${NCBI_C_LIBPATH} ${_c_libs})
    set(NCBI_COMPONENT_NCBI_C_DEFINES HAVE_NCBI_C=1)
    list(APPEND NCBI_ALL_COMPONENTS NCBI_C)
else()
  set(NCBI_COMPONENT_NCBI_C_FOUND NO)
endif()

#############################################################################
# STACKTRACE
if(EXISTS ${NCBI_ThirdParty_BACKWARD}/include)
    set(LIBBACKWARD_INCLUDE ${NCBI_ThirdParty_BACKWARD}/include)
    set(HAVE_LIBBACKWARD_CPP YES)
endif()
find_library(LIBBACKWARD_LIBS NAMES backward HINTS ${NCBI_ThirdParty_BACKWARD}/lib)

if(EXISTS ${NCBI_ThirdParty_UNWIND}/include)
    set(LIBUNWIND_INCLUDE ${NCBI_ThirdParty_UNWIND}/include)
    set(HAVE_LIBUNWIND YES)
    find_library(LIBUNWIND_LIBS NAMES unwind HINTS "${NCBI_ThirdParty_UNWIND}/lib" )
endif()

find_library(LIBDW_LIBS NAMES dw)
if (LIBDW_LIBS)
    set(HAVE_LIBDW YES)
endif()

if(HAVE_LIBBACKWARD_CPP OR HAVE_LIBUNWIND OR HAVE_LIBDW)
    set(NCBI_COMPONENT_STACKTRACE_FOUND YES)
    set(NCBI_COMPONENT_STACKTRACE_INCLUDE ${LIBBACKWARD_INCLUDE} ${LIBUNWIND_INCLUDE})
    set(NCBI_COMPONENT_STACKTRACE_LIBS ${LIBBACKWARD_LIBS} ${LIBUNWIND_LIBS} ${LIBDW_LIBS})
else()
    set(NCBI_COMPONENT_STACKTRACE_FOUND NO)
endif()

##############################################################################
# UUID
NCBI_find_library(UUID uuid)

##############################################################################
# CURL
NCBI_find_library(CURL curl)

#############################################################################
# LMDB
NCBI_define_component(LMDB lmdb)
if(NOT NCBI_COMPONENT_LMDB_FOUND)
    set(NCBI_COMPONENT_LMDB_FOUND ${NCBI_COMPONENT_LocalLMDB_FOUND})
    set(NCBI_COMPONENT_LMDB_INCLUDE ${NCBI_COMPONENT_LocalLMDB_INCLUDE})
    set(NCBI_COMPONENT_LMDB_NCBILIB ${NCBI_COMPONENT_LocalLMDB_NCBILIB})
endif()

#############################################################################
# PCRE
if(NOT USE_LOCAL_PCRE)
    NCBI_find_library(PCRE pcre)
endif()
if(NOT NCBI_COMPONENT_PCRE_FOUND)
    set(NCBI_COMPONENT_PCRE_FOUND ${NCBI_COMPONENT_LocalPCRE_FOUND})
    set(NCBI_COMPONENT_PCRE_INCLUDE ${NCBI_COMPONENT_LocalPCRE_INCLUDE})
    set(NCBI_COMPONENT_PCRE_NCBILIB ${NCBI_COMPONENT_LocalPCRE_NCBILIB})
endif()

#############################################################################
# Z
NCBI_find_library(Z z)
if(NOT NCBI_COMPONENT_Z_FOUND)
    set(NCBI_COMPONENT_Z_FOUND ${NCBI_COMPONENT_LocalZ_FOUND})
    set(NCBI_COMPONENT_Z_INCLUDE ${NCBI_COMPONENT_LocalZ_INCLUDE})
    set(NCBI_COMPONENT_Z_NCBILIB ${NCBI_COMPONENT_LocalZ_NCBILIB})
endif()

#############################################################################
# BZ2
if(NOT USE_LOCAL_BZLIB)
    NCBI_find_library(BZ2 bz2)
endif()
if(NOT NCBI_COMPONENT_BZ2_FOUND)
    set(NCBI_COMPONENT_BZ2_FOUND ${NCBI_COMPONENT_LocalBZ2_FOUND})
    set(NCBI_COMPONENT_BZ2_INCLUDE ${NCBI_COMPONENT_LocalBZ2_INCLUDE})
    set(NCBI_COMPONENT_BZ2_NCBILIB ${NCBI_COMPONENT_LocalBZ2_NCBILIB})
endif()

#############################################################################
# LZO
NCBI_define_component(LZO lzo2)

#############################################################################
# BOOST
include(${NCBI_TREE_CMAKECFG}/CMakeChecks.boost.cmake)

#############################################################################
# Boost.Test.Included
if(Boost_FOUND)
  set(NCBI_COMPONENT_Boost.Test.Included_FOUND YES)
  set(NCBI_COMPONENT_Boost.Test.Included_INCLUDE ${Boost_INCLUDE_DIRS})
else()
  set(NCBI_COMPONENT_Boost.Test.Included_FOUND NO)
endif()

#############################################################################
# Boost.Test
if(Boost_FOUND)
  set(NCBI_COMPONENT_Boost.Test_FOUND YES)
  set(NCBI_COMPONENT_Boost.Test_INCLUDE ${Boost_INCLUDE_DIRS})
  set(NCBI_COMPONENT_Boost.Test_LIBS    ${Boost_LIBRARIES})
else()
  set(NCBI_COMPONENT_Boost.Test_FOUND NO)
endif()

#############################################################################
# Boost.Spirit
if(Boost_FOUND)
  set(NCBI_COMPONENT_Boost.Spirit_FOUND YES)
  set(NCBI_COMPONENT_Boost.Spirit_INCLUDE ${Boost_INCLUDE_DIRS})
  set(NCBI_COMPONENT_Boost.Spirit_LIBS    ${Boost_LIBRARIES})
else()
  set(NCBI_COMPONENT_Boost.Spirit_FOUND NO)
endif()

#############################################################################
# Boost
if(Boost_FOUND)
  set(NCBI_COMPONENT_Boost_FOUND YES)
  set(NCBI_COMPONENT_Boost_INCLUDE ${Boost_INCLUDE_DIRS})
  set(NCBI_COMPONENT_Boost_LIBS    ${Boost_LIBRARIES})
    list(APPEND NCBI_ALL_COMPONENTS Boost)
else()
  set(NCBI_COMPONENT_Boost_FOUND NO)
endif()

#############################################################################
# JPEG
NCBI_find_library(JPEG jpeg)

#############################################################################
# PNG
NCBI_find_library(PNG png)

#############################################################################
# GIF
NCBI_find_library(GIF gif)

#############################################################################
# TIFF
NCBI_find_library(TIFF tiff)

#############################################################################
# TLS
set(NCBI_COMPONENT_TLS_FOUND YES)
list(APPEND NCBI_ALL_COMPONENTS TLS)

#############################################################################
# FASTCGI
NCBI_define_component(FASTCGI fcgi)

#############################################################################
# SQLITE3
NCBI_define_component(SQLITE3 sqlite3)
if(NCBI_COMPONENT_SQLITE3_FOUND)
    check_symbol_exists(sqlite3_unlock_notify ${NCBI_COMPONENT_SQLITE3_INCLUDE}/sqlite3.h HAVE_SQLITE3_UNLOCK_NOTIFY)
    check_include_file(sqlite3async.h HAVE_SQLITE3ASYNC_H -I${NCBI_COMPONENT_SQLITE3_INCLUDE})
endif()

#############################################################################
# BerkeleyDB
NCBI_define_component(BerkeleyDB db)
if(NCBI_COMPONENT_BerkeleyDB_FOUND)
    set(HAVE_BERKELEY_DB 1)
    set(HAVE_BDB         1)
    set(HAVE_BDB_CACHE   1)
endif()

#############################################################################
# ODBC
set(NCBI_COMPONENT_ODBC_FOUND NO)

#############################################################################
# MySQL
find_external_library(Mysql INCLUDES mysql/mysql.h LIBS mysqlclient)
if(MYSQL_FOUND)
    set(NCBI_COMPONENT_MySQL_FOUND YES)
    set(NCBI_COMPONENT_MySQL_INCLUDE ${MYSQL_INCLUDE})
    set(NCBI_COMPONENT_MySQL_LIBS    ${MYSQL_LIBS})
    list(APPEND NCBI_ALL_COMPONENTS MySQL)
else()
  set(NCBI_COMPONENT_MySQL_FOUND NO)
endif()

#############################################################################
# Sybase
#NCBI_define_component(Sybase sybdb64 sybblk_r64 sybct_r64 sybcs_r64 sybtcl_r64 sybcomn_r64 sybintl_r64 sybunic64)
NCBI_define_component(Sybase          sybblk_r64 sybct_r64 sybcs_r64 sybtcl_r64 sybcomn_r64 sybintl_r64 sybunic64)
if (NCBI_COMPONENT_Sybase_FOUND)
    set(NCBI_COMPONENT_Sybase_DEFINES SYB_LP64)
    set(SYBASE_PATH ${NCBI_ThirdParty_Sybase})
    set(SYBASE_LCL_PATH "")
endif()

#############################################################################
# PYTHON
set(NCBI_COMPONENT_PYTHON_FOUND NO)

#############################################################################
# VDB
find_external_library(VDB INCLUDES sra/sradb.h LIBS ncbi-vdb
    INCLUDE_HINTS ${NCBI_ThirdParty_VDB}/interfaces
    LIBS_HINTS    ${NCBI_ThirdParty_VDB}/linux/release/x86_64/lib
)
if(VDB_FOUND)
    set(NCBI_COMPONENT_VDB_FOUND YES)
    set(NCBI_COMPONENT_VDB_INCLUDE 
        ${VDB_INCLUDE} 
        ${VDB_INCLUDE}/os/linux 
        ${VDB_INCLUDE}/os/unix
        ${VDB_INCLUDE}/cc/gcc/x86_64
        ${VDB_INCLUDE}/cc/gcc
    )
    set(NCBI_COMPONENT_VDB_LIBS    ${VDB_LIBS})
    list(APPEND NCBI_ALL_COMPONENTS VDB)
else()
    set(NCBI_COMPONENT_VDB_FOUND NO)
endif()

##############################################################################
# wxWidgets
include(${NCBI_TREE_CMAKECFG}/CMakeChecks.wxwidgets.cmake)
if (WXWIDGETS_FOUND)
    set(NCBI_COMPONENT_wxWidgets_FOUND   YES)
    set(NCBI_COMPONENT_wxWidgets_INCLUDE ${WXWIDGETS_INCLUDE})
    set(NCBI_COMPONENT_wxWidgets_LIBS    ${WXWIDGETS_LIBS})
    set(HAVE_WXWIDGETS 1)
    if(BUILD_SHARED_LIBS)
        set(NCBI_COMPONENT_wxWidgets_DEFINES __WXGTK__  WXUSINGDLL wxDEBUG_LEVEL=0)
    else()
        set(NCBI_COMPONENT_wxWidgets_DEFINES __WXGTK__ wxDEBUG_LEVEL=0)
    endif()
    list(APPEND NCBI_ALL_COMPONENTS wxWidgets)
endif()

#############################################################################
# XML
NCBI_define_component(XML xml2)
if (NCBI_COMPONENT_XML_FOUND)
    set(NCBI_COMPONENT_XML_INCLUDE ${NCBI_COMPONENT_XML_INCLUDE} ${NCBI_COMPONENT_XML_INCLUDE}/libxml2)
endif()

#############################################################################
# XSLT
NCBI_define_component(XSLT xslt exslt)
if (NCBI_COMPONENT_XSLT_FOUND)
    set(NCBI_COMPONENT_XSLT_INCLUDE ${NCBI_COMPONENT_XSLT_INCLUDE} ${NCBI_COMPONENT_XSLT_INCLUDE}/libxml2)
    set(NCBI_COMPONENT_XSLTPROCTOOL_FOUND YES)
    set(NCBI_XSLTPROCTOOL ${NCBI_ThirdParty_XSLT}/bin/xsltproc)
endif()

#############################################################################
# EXSLT
NCBI_define_component(EXSLT xslt exslt)
if (NCBI_COMPONENT_EXSLT_FOUND)
    set(NCBI_COMPONENT_EXSLT_INCLUDE ${NCBI_COMPONENT_EXSLT_INCLUDE} ${NCBI_COMPONENT_EXSLT_INCLUDE}/libxml2)
endif()

#############################################################################
# XLSXWRITER
NCBI_define_component(XLSXWRITER xlsxwriter)

#############################################################################
# LAPACK
check_include_file(lapacke.h HAVE_LAPACKE_H)
check_include_file(lapacke/lapacke.h HAVE_LAPACKE_LAPACKE_H)
check_include_file(Accelerate/Accelerate.h HAVE_ACCELERATE_ACCELERATE_H)
find_package(LAPACK)
if (LAPACK_FOUND)
    set(LAPACK_INCLUDE ${LAPACK_INCLUDE_DIRS})
    set(LAPACK_LIBS ${LAPACK_LIBRARIES})
else ()
    find_library(LAPACK_LIBS lapack blas)
endif ()
if(LAPACK_FOUND)
    set(NCBI_COMPONENT_LAPACK_FOUND YES)
    set(NCBI_COMPONENT_LAPACK_INCLUDE ${LAPACK_INCLUDE})
    set(NCBI_COMPONENT_LAPACK_LIBS ${LAPACK_LIBS})
    list(APPEND NCBI_ALL_COMPONENTS LAPACK)
else()
  set(NCBI_COMPONENT_LAPACK_FOUND NO)
endif()

#############################################################################
# SAMTOOLS
find_external_library(samtools INCLUDES bam.h  LIBS bam HINTS "${NCBI_TOOLS_ROOT}/samtools")
if(SAMTOOLS_FOUND)
    set(NCBI_COMPONENT_SAMTOOLS_FOUND YES)
    set(NCBI_COMPONENT_SAMTOOLS_INCLUDE ${SAMTOOLS_INCLUDE})
    set(NCBI_COMPONENT_SAMTOOLS_LIBS    ${SAMTOOLS_LIBS})
    list(APPEND NCBI_ALL_COMPONENTS SAMTOOLS)
else()
    set(NCBI_COMPONENT_SAMTOOLS_FOUND NO)
endif()

#############################################################################
# FreeType
NCBI_find_library(FreeType freetype)
if(NCBI_COMPONENT_FreeType_FOUND)
    set(NCBI_COMPONENT_FreeType_INCLUDE "/usr/include/freetype2")
endif()

#############################################################################
# FTGL
NCBI_define_component(FTGL ftgl)

#############################################################################
# GLEW
NCBI_define_component(GLEW GLEW)
if(NCBI_COMPONENT_GLEW_FOUND)
    if(BUILD_SHARED_LIBS)
        set(NCBI_COMPONENT_GLEW_DEFINES GLEW_MX)
    else()
        set(NCBI_COMPONENT_GLEW_DEFINES GLEW_MX GLEW_STATIC)
    endif()
endif()

##############################################################################
# OpenGL
NCBI_define_component(OpenGL GLU GL)
if(NCBI_COMPONENT_OpenGL_FOUND)
    set(NCBI_COMPONENT_OpenGL_LIBS ${NCBI_COMPONENT_OpenGL_LIBS}  -lXmu -lXt -lXext -lX11)
endif()

##############################################################################
# OSMesa
NCBI_define_component(OSMesa OSMesa GLU GL)
if(NCBI_COMPONENT_OSMesa_FOUND)
    set(NCBI_COMPONENT_OSMesa_LIBS ${NCBI_COMPONENT_OSMesa_LIBS}  -lXmu -lXt -lXext -lX11)
endif()

#############################################################################
# XERCES
NCBI_define_component(XERCES xerces-c)

##############################################################################
# GRPC/PROTOBUF
set(NCBI_PROTOC_APP "${NCBI_ThirdParty_GRPC}/${CMAKE_BUILD_TYPE}${NCBI_PlatformBits}/bin/protoc")
set(NCBI_GRPC_PLUGIN "${NCBI_ThirdParty_GRPC}/${CMAKE_BUILD_TYPE}${NCBI_PlatformBits}/bin/grpc_cpp_plugin")

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  set(_suffix "d")
else()
  set(_suffix "")
endif()
NCBI_define_component(PROTOBUF protobuf${_suffix})
NCBI_define_component(GRPC grpc++ grpc gpr protobuf${_suffix} cares address_sorting boringssl boringcrypto)

#############################################################################
# XALAN
NCBI_define_component(XALAN xalan-c xalanMsg)

##############################################################################
# GCRYPT
NCBI_define_component(GCRYPT gcrypt)
NCBI_define_component(GPG    gpg-error)
if(NCBI_COMPONENT_GCRYPT_FOUND)
    set(NCBI_COMPONENT_GCRYPT_INCLUDE ${NCBI_COMPONENT_GCRYPT_INCLUDE} ${NCBI_COMPONENT_GPG_INCLUDE})
    set(NCBI_COMPONENT_GCRYPT_LIBS    ${NCBI_COMPONENT_GCRYPT_LIBS}    ${NCBI_COMPONENT_GPG_LIBS})
endif()

##############################################################################
# PERL
find_package(PerlLibs)
if (PERLLIBS_FOUND)
    set(NCBI_COMPONENT_PERL_FOUND   YES)
    set(NCBI_COMPONENT_PERL_INCLUDE ${PERL_INCLUDE_PATH})
    set(NCBI_COMPONENT_PERL_LIBS    ${PERL_LIBRARY})
    list(APPEND NCBI_ALL_COMPONENTS PERL)
endif()

#############################################################################
# OpenSSL
NCBI_find_library(OpenSSL ssl crypto)

#############################################################################
# MSGSL  (Microsoft Guidelines Support Library)
NCBI_define_component(MSGSL)

#############################################################################
# SGE  (Sun Grid Engine)
find_external_library(SGE INCLUDES drmaa.h LIBS drmaa
    INCLUDE_HINTS "${NCBI_ThirdParty_SGE}/include"
    LIBS_HINTS "${NCBI_ThirdParty_SGE}/lib/lx-amd64/")
if (SGE_FOUND)
    set(NCBI_COMPONENT_SGE_FOUND YES)
    set(NCBI_COMPONENT_SGE_INCLUDE ${SGE_INCLUDE})
    set(NCBI_COMPONENT_SGE_LIBS    ${SGE_LIBS})
    set(HAVE_LIBSGE 1)
    list(APPEND NCBI_ALL_COMPONENTS SGE)
endif()

#############################################################################
# MONGOCXX
find_package(MongoCXX)
if (MONGOCXX_FOUND)
    set(NCBI_COMPONENT_MONGOCXX_FOUND YES)
    set(NCBI_COMPONENT_MONGOCXX_INCLUDE ${MONGOCXX_INCLUDE_DIRS} ${BSONCXX_INCLUDE_DIRS})
    set(NCBI_COMPONENT_MONGOCXX_LIBS    ${MONGOCXX_LIBPATH} ${MONGOCXX_LIBRARIES} ${BSONCXX_LIBRARIES})
    list(APPEND NCBI_ALL_COMPONENTS MONGOCXX)
endif()

#############################################################################
# LEVELDB
NCBI_define_component(LEVELDB leveldb)

#############################################################################
# WGMLST
find_package(SKESA)
if (WGMLST_FOUND)
    set(NCBI_COMPONENT_WGMLST_FOUND YES)
    set(NCBI_COMPONENT_WGMLST_INCLUDE ${WGMLST_INCLUDE_DIRS})
    set(NCBI_COMPONENT_WGMLST_LIBS    ${WGMLST_LIBPATH} ${WGMLST_LIBRARIES})
    list(APPEND NCBI_ALL_COMPONENTS WGMLST)
endif()

#############################################################################
# GLPK
find_external_library(glpk INCLUDES glpk.h LIBS glpk
    HINTS "/usr/local/glpk/4.45")
if(GLPK_FOUND)
    set(NCBI_COMPONENT_GLPK_FOUND YES)
    set(NCBI_COMPONENT_GLPK_INCLUDE ${GLPK_INCLUDE})
    set(NCBI_COMPONENT_GLPK_LIBS    ${GLPK_LIBS})
    list(APPEND NCBI_ALL_COMPONENTS GLPK)
endif()


