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

set(NCBI_REQUIRE_unix_FOUND YES)
list(APPEND NCBI_ALL_REQUIRES unix)
if(NOT APPLE)
    set(NCBI_REQUIRE_Linux_FOUND YES)
    list(APPEND NCBI_ALL_REQUIRES Linux)
endif()
option(USE_LOCAL_BZLIB "Use a local copy of libbz2")
option(USE_LOCAL_PCRE "Use a local copy of libpcre")
#to debug
#set(NCBI_TRACE_COMPONENT_GRPC ON)
#############################################################################
# common settings
set(NCBI_TOOLS_ROOT $ENV{NCBI})
set(NCBI_OPT_ROOT  /opt/ncbi/64)
set(NCBI_PlatformBits 64)

include(CheckLibraryExists)
include(${NCBI_TREE_CMAKECFG}/FindExternalLibrary.cmake)
find_package(PkgConfig REQUIRED)

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
set(NCBI_ThirdParty_BACKWARD      ${NCBI_TOOLS_ROOT}/backward-cpp-1.3.20180206-44ae960 CACHE PATH "BACKWARD root")
set(NCBI_ThirdParty_UNWIND        ${NCBI_TOOLS_ROOT}/libunwind-1.1 CACHE PATH "UNWIND root")
set(NCBI_ThirdParty_LMDB          ${NCBI_TOOLS_ROOT}/lmdb-0.9.24 CACHE PATH "LMDB root")
set(NCBI_ThirdParty_LZO           ${NCBI_TOOLS_ROOT}/lzo-2.05 CACHE PATH "LZO root")
set(NCBI_ThirdParty_FASTCGI       ${NCBI_TOOLS_ROOT}/fcgi-2.4.0 CACHE PATH "FASTCGI root")
set(NCBI_ThirdParty_FASTCGI_SHLIB ${NCBI_ThirdParty_FASTCGI})
set(NCBI_ThirdParty_SQLITE3       ${NCBI_TOOLS_ROOT}/sqlite-3.26.0-ncbi1 CACHE PATH "SQLITE3 root")
set(NCBI_ThirdParty_BerkeleyDB    ${NCBI_TOOLS_ROOT}/BerkeleyDB-4.6.21.1 CACHE PATH "BerkeleyDB root")
set(NCBI_ThirdParty_SybaseNetPath "/opt/sybase/clients/16.0-64bit" CACHE PATH "SybaseNetPath")
set(NCBI_ThirdParty_SybaseLocalPath "" CACHE PATH "SybaseLocalPath")
set(NCBI_ThirdParty_PYTHON        "/opt/python-3.8" CACHE PATH "PYTHON root")
set(NCBI_ThirdParty_VDB           ${NCBI_OPT_ROOT}/trace_software/vdb/vdb-versions/cxx_toolkit/2 CACHE PATH "VDB root")
set(NCBI_ThirdParty_XML           ${NCBI_TOOLS_ROOT}/libxml-2.7.8 CACHE PATH "XML root")
set(NCBI_ThirdParty_XSLT          ${NCBI_TOOLS_ROOT}/libxml-2.7.8 CACHE PATH "XSLT root")
set(NCBI_ThirdParty_EXSLT         ${NCBI_ThirdParty_XSLT})
set(NCBI_ThirdParty_XLSXWRITER    ${NCBI_TOOLS_ROOT}/libxlsxwriter-0.6.9 CACHE PATH "XLSXWRITER root")
set(NCBI_ThirdParty_FTGL          ${NCBI_TOOLS_ROOT}/ftgl-2.1.3-rc5 CACHE PATH "FTGL root")
set(NCBI_ThirdParty_GLEW          ${NCBI_TOOLS_ROOT}/glew-1.5.8 CACHE PATH "GLEW root")
set(NCBI_ThirdParty_OpenGL        ${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2 CACHE PATH "OpenGL root")
set(NCBI_ThirdParty_OSMesa        ${NCBI_ThirdParty_OpenGL})
set(NCBI_ThirdParty_XERCES        ${NCBI_TOOLS_ROOT}/xerces-3.1.2 CACHE PATH "XERCES root")
set(NCBI_ThirdParty_GRPC          ${NCBI_TOOLS_ROOT}/grpc-1.28.1-ncbi1 CACHE PATH "GRPC root")
set(NCBI_ThirdParty_Boring        ${NCBI_ThirdParty_GRPC})
set(NCBI_ThirdParty_PROTOBUF      ${NCBI_TOOLS_ROOT}/grpc-1.28.1-ncbi1 CACHE PATH "PROTOBUF root")
set(NCBI_ThirdParty_XALAN         ${NCBI_TOOLS_ROOT}/xalan-1.11 CACHE PATH "XALAN root")
set(NCBI_ThirdParty_GPG           ${NCBI_TOOLS_ROOT}/libgpg-error-1.6 CACHE PATH "GPG root")
set(NCBI_ThirdParty_GCRYPT        ${NCBI_TOOLS_ROOT}/libgcrypt-1.4.3 CACHE PATH "GCRYPT root")
set(NCBI_ThirdParty_MSGSL         ${NCBI_TOOLS_ROOT}/msgsl-0.0.20171114-1c95f94 CACHE PATH "MSGSL root")
set(NCBI_ThirdParty_SGE           "/netmnt/gridengine/current" CACHE PATH "SGE root")
set(NCBI_ThirdParty_MONGOCXX      ${NCBI_TOOLS_ROOT}/mongodb-3.4.0 CACHE PATH "MONGOCXX root")
set(NCBI_ThirdParty_MONGOC        ${NCBI_TOOLS_ROOT}/mongo-c-driver-1.14.0 CACHE PATH "MONGOC root")
set(NCBI_ThirdParty_LEVELDB       ${NCBI_TOOLS_ROOT}/leveldb-1.21 CACHE PATH "LEVELDB root")
if (APPLE)
  set(NCBI_ThirdParty_wxWidgets     ${NCBI_TOOLS_ROOT}/wxWidgets-3.1.4-ncbi1 CACHE PATH "wxWidgets root")
else(APPLE)
  set(NCBI_ThirdParty_wxWidgets     ${NCBI_TOOLS_ROOT}/wxWidgets-3.1.3-ncbi1 CACHE PATH "wxWidgets root")
endif(APPLE)
set(NCBI_ThirdParty_UV            ${NCBI_TOOLS_ROOT}/libuv-1.35.0 CACHE PATH "UV root")
set(NCBI_ThirdParty_NGHTTP2       ${NCBI_TOOLS_ROOT}/nghttp2-1.40.0 CACHE PATH "NGHTTP2 root")
set(NCBI_ThirdParty_GL2PS         ${NCBI_TOOLS_ROOT}/gl2ps-1.4.0 CACHE PATH "GL2PS root")
set(NCBI_ThirdParty_GMOCK         ${NCBI_TOOLS_ROOT}/googletest-1.8.1 CACHE PATH "GMOCK root")
set(NCBI_ThirdParty_GTEST         ${NCBI_TOOLS_ROOT}/googletest-1.8.1 CACHE PATH "GTEST root")
set(NCBI_ThirdParty_CASSANDRA     ${NCBI_TOOLS_ROOT}/datastax-cpp-driver-2.9.0-ncbi1 CACHE PATH "CASSANDRA root")
set(NCBI_ThirdParty_H2O           ${NCBI_TOOLS_ROOT}/h2o-2.2.5 CACHE PATH "H2O root")

#############################################################################
#############################################################################

#############################################################################
# in-house-resources
if(EXISTS "${NCBI_TOOLS_ROOT}/.ncbirc" AND EXISTS "/am/ncbiapdata/test_data")
    set(NCBITEST_TESTDATA_PATH "/am/ncbiapdata/test_data")
    set(NCBI_REQUIRE_in-house-resources_FOUND YES)
    list(APPEND NCBI_ALL_REQUIRES in-house-resources)
endif()

#############################################################################
# NCBI_C
if(NOT NCBI_COMPONENT_NCBI_C_DISABLED)
    set(NCBI_C_ROOT "${NCBI_TOOLS_ROOT}/ncbi")
    string(REGEX MATCH "NCBI_INT8_GI|NCBI_STRICT_GI" INT8GI_FOUND "${CMAKE_CXX_FLAGS}")
    if(NOT "${INT8GI_FOUND}" STREQUAL "")
        if (EXISTS "${NCBI_C_ROOT}/ncbi.gi64")
            set(NCBI_CTOOLKIT_PATH "${NCBI_C_ROOT}/ncbi.gi64")
        elseif (EXISTS "${NCBI_C_ROOT}.gi64")
            set(NCBI_CTOOLKIT_PATH "${NCBI_C_ROOT}.gi64")
        else ()
            set(NCBI_CTOOLKIT_PATH "${NCBI_C_ROOT}")
        endif ()
    else()
        set(NCBI_CTOOLKIT_PATH "${NCBI_C_ROOT}")
    endif()

    if(EXISTS "${NCBI_CTOOLKIT_PATH}/include64" AND EXISTS "${NCBI_CTOOLKIT_PATH}/lib64")
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
    else()
        set(HAVE_NCBI_C NO)
    endif()

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
        message("NOT FOUND NCBI_C")
    endif()
else()
    message("DISABLED NCBI_C")
endif()

#############################################################################
# BACKWARD, UNWIND
if(NOT NCBI_COMPONENT_BACKWARD_DISABLED)
    if(EXISTS ${NCBI_ThirdParty_BACKWARD}/include)
        set(LIBBACKWARD_INCLUDE ${NCBI_ThirdParty_BACKWARD}/include)
        set(HAVE_LIBBACKWARD_CPP YES)
        set(NCBI_COMPONENT_BACKWARD_FOUND YES)
        set(NCBI_COMPONENT_BACKWARD_INCLUDE ${LIBBACKWARD_INCLUDE})
        list(APPEND NCBI_ALL_COMPONENTS BACKWARD)
    else()
        message("NOT FOUND BACKWARD")
    endif()
    find_library(LIBBACKWARD_LIBS NAMES backward HINTS ${NCBI_ThirdParty_BACKWARD}/lib)
    find_library(LIBDW_LIBS NAMES dw)
    if (LIBDW_LIBS)
        set(HAVE_LIBDW YES)
    endif()
    if(HAVE_LIBBACKWARD_CPP AND HAVE_LIBDW)
        set(NCBI_COMPONENT_BACKWARD_LIBS ${LIBDW_LIBS})
#        set(NCBI_COMPONENT_BACKWARD_LIBS ${LIBBACKWARD_LIBS} ${LIBDW_LIBS})
    endif()
else(NOT NCBI_COMPONENT_BACKWARD_DISABLED)
    message("DISABLED BACKWARD")
endif(NOT NCBI_COMPONENT_BACKWARD_DISABLED)

if(NOT NCBI_COMPONENT_UNWIND_DISABLED)
    if(EXISTS ${NCBI_ThirdParty_UNWIND}/include)
        set(LIBUNWIND_INCLUDE ${NCBI_ThirdParty_UNWIND}/include)
        set(HAVE_LIBUNWIND YES)
        find_library(LIBUNWIND_LIBS NAMES unwind HINTS "${NCBI_ThirdParty_UNWIND}/lib" )
        set(NCBI_COMPONENT_UNWIND_FOUND YES)
        set(NCBI_COMPONENT_UNWIND_INCLUDE ${LIBUNWIND_INCLUDE})
        set(NCBI_COMPONENT_UNWIND_LIBS ${LIBUNWIND_LIBS})
        list(APPEND NCBI_ALL_COMPONENTS UNWIND)
    else()
        message("NOT FOUND UNWIND")
    endif()
else(NOT NCBI_COMPONENT_UNWIND_DISABLED)
    message("DISABLED UNWIND")
endif(NOT NCBI_COMPONENT_UNWIND_DISABLED)

##############################################################################
# UUID
NCBI_find_library(UUID uuid)

##############################################################################
# CURL
if(ON)
    NCBI_find_package(CURL CURL)
else()
    NCBI_find_library(CURL curl)
endif()

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
    if(OFF)
        NCBI_find_package(PCRE libpcre)
    else()
        NCBI_find_library(PCRE pcre)
    endif()
endif()
if(NOT NCBI_COMPONENT_PCRE_FOUND)
    set(NCBI_COMPONENT_PCRE_FOUND ${NCBI_COMPONENT_LocalPCRE_FOUND})
    set(NCBI_COMPONENT_PCRE_INCLUDE ${NCBI_COMPONENT_LocalPCRE_INCLUDE})
    set(NCBI_COMPONENT_PCRE_NCBILIB ${NCBI_COMPONENT_LocalPCRE_NCBILIB})
endif()

#############################################################################
# Z
if(ON)
    NCBI_find_package(Z ZLIB)
else()
    NCBI_find_library(Z z)
endif()
if(NOT NCBI_COMPONENT_Z_FOUND)
    set(NCBI_COMPONENT_Z_FOUND ${NCBI_COMPONENT_LocalZ_FOUND})
    set(NCBI_COMPONENT_Z_INCLUDE ${NCBI_COMPONENT_LocalZ_INCLUDE})
    set(NCBI_COMPONENT_Z_NCBILIB ${NCBI_COMPONENT_LocalZ_NCBILIB})
endif()

#############################################################################
# BZ2
if(NOT USE_LOCAL_BZLIB)
    if(ON)
        NCBI_find_package(BZ2 BZip2)
    else()
        NCBI_find_library(BZ2 bz2)
    endif()
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
if(ON)
    NCBI_find_package(JPEG JPEG)
else()
    NCBI_find_library(JPEG jpeg)
endif()

#############################################################################
# PNG
if(ON)
    NCBI_find_package(PNG PNG)
else()
    NCBI_find_library(PNG png)
endif()

#############################################################################
# GIF
if(ON)
    NCBI_find_package(GIF GIF)
else()
    NCBI_find_library(GIF gif)
endif()

#############################################################################
# TIFF
if(ON)
    NCBI_find_package(TIFF TIFF)
else()
    NCBI_find_library(TIFF tiff)
endif()

#############################################################################
# TLS
set(NCBI_COMPONENT_TLS_FOUND YES)
list(APPEND NCBI_ALL_COMPONENTS TLS)

#############################################################################
# FASTCGI
NCBI_define_component(FASTCGI fcgi)

#############################################################################
# SQLITE3
if(ON)
    NCBI_find_package(SQLITE3 sqlite3)
endif()
if(NOT NCBI_COMPONENT_SQLITE3_FOUND)
    NCBI_define_component(SQLITE3 sqlite3)
endif()
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
if(OFF)
    NCBI_find_package(ODBC ODBC)
else()
    set(NCBI_COMPONENT_ODBC_FOUND NO)
endif()

#############################################################################
# MySQL
if(NOT NCBI_COMPONENT_MySQL_DISABLED)
    find_external_library(Mysql INCLUDES mysql/mysql.h LIBS mysqlclient)
    if(MYSQL_FOUND)
        set(NCBI_COMPONENT_MySQL_FOUND YES)
        set(NCBI_COMPONENT_MySQL_INCLUDE ${MYSQL_INCLUDE})
        set(NCBI_COMPONENT_MySQL_LIBS    ${MYSQL_LIBS})
        list(APPEND NCBI_ALL_COMPONENTS MySQL)
    else()
      set(NCBI_COMPONENT_MySQL_FOUND NO)
    endif()
else()
    message("DISABLED MySQL")
endif()

#############################################################################
# Sybase

file(GLOB _files LIST_DIRECTORIES TRUE "${NCBI_ThirdParty_SybaseNetPath}/OCS*")
if(_files)
    list(GET _files 0 NCBI_ThirdParty_Sybase)
endif()
#NCBI_define_component(Sybase sybdb64 sybblk_r64 sybct_r64 sybcs_r64 sybtcl_r64 sybcomn_r64 sybintl_r64 sybunic64)
NCBI_define_component(Sybase          sybblk_r64 sybct_r64 sybcs_r64 sybtcl_r64 sybcomn_r64 sybintl_r64 sybunic64)
if (NCBI_COMPONENT_Sybase_FOUND)
    set(NCBI_COMPONENT_Sybase_DEFINES ${NCBI_COMPONENT_Sybase_DEFINES} SYB_LP64)
    set(SYBASE_PATH ${NCBI_ThirdParty_SybaseNetPath})
    set(SYBASE_LCL_PATH "${NCBI_ThirdParty_SybaseLocalPath}")
endif()

#############################################################################
# PYTHON
NCBI_define_component(PYTHON python3.8 python3)
if (NCBI_COMPONENT_PYTHON_FOUND)
    set(NCBI_COMPONENT_PYTHON_INCLUDE ${NCBI_COMPONENT_PYTHON_INCLUDE}/python3.8)
endif()

#############################################################################
# VDB
if(NOT NCBI_COMPONENT_VDB_DISABLED)
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
        set(HAVE_NCBI_VDB 1)
        list(APPEND NCBI_ALL_COMPONENTS VDB)
    else()
        set(NCBI_COMPONENT_VDB_FOUND NO)
    endif()
else()
    message("DISABLED VDB")
endif()

##############################################################################
# wxWidgets
if(OFF)
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
else()
    if(ON)
        NCBI_find_package(GTK2 GTK2)
        NCBI_find_package(FONTCONFIG Fontconfig)
        set(_wx_ver 3.1)
        NCBI_define_component(wxWidgets
            wx_gtk2_gl-${_wx_ver}
            wx_gtk2_richtext-${_wx_ver}
            wx_gtk2_aui-${_wx_ver}
            wx_gtk2_propgrid-${_wx_ver}
            wx_gtk2_xrc-${_wx_ver}
            wx_gtk2_html-${_wx_ver}
            wx_gtk2_qa-${_wx_ver}
            wx_gtk2_adv-${_wx_ver}
            wx_gtk2_core-${_wx_ver}
            wx_base_xml-${_wx_ver}
            wx_base_net-${_wx_ver}
            wx_base-${_wx_ver}
            wxscintilla-${_wx_ver}
        )
        if(NCBI_COMPONENT_wxWidgets_FOUND)
            list(GET NCBI_COMPONENT_wxWidgets_LIBS 0 _lib)
            get_filename_component(_libdir ${_lib} DIRECTORY)
            set(NCBI_COMPONENT_wxWidgets_INCLUDE ${NCBI_COMPONENT_wxWidgets_INCLUDE}/wx-${_wx_ver} ${_libdir}/wx/include/gtk2-ansi-${_wx_ver} ${NCBI_COMPONENT_GTK2_INCLUDE})
            set(NCBI_COMPONENT_wxWidgets_LIBS    ${NCBI_COMPONENT_wxWidgets_LIBS} ${NCBI_COMPONENT_FONTCONFIG_LIBS} ${NCBI_COMPONENT_GTK2_LIBS} -lXxf86vm -lSM -lexpat)
            if(BUILD_SHARED_LIBS)
                set(NCBI_COMPONENT_wxWidgets_DEFINES __WXGTK__  WXUSINGDLL wxDEBUG_LEVEL=0)
            else()
                set(NCBI_COMPONENT_wxWidgets_DEFINES __WXGTK__ wxDEBUG_LEVEL=0)
            endif()
        endif()
    else()
        find_package(GTK2)
        NCBI_find_library(FONTCONFIG fontconfig)
        if (GTK2_FOUND)
            NCBI_define_component(wxWidgets
                wx_gtk2_gl-3.1
                wx_gtk2_richtext-3.1
                wx_gtk2_aui-3.1
                wx_gtk2_propgrid-3.1
                wx_gtk2_xrc-3.1
                wx_gtk2_html-3.1
                wx_gtk2_qa-3.1
                wx_gtk2_adv-3.1
                wx_gtk2_core-3.1
                wx_base_xml-3.1
                wx_base_net-3.1
                wx_base-3.1
                wxscintilla-3.1
            )
            if(NCBI_COMPONENT_wxWidgets_FOUND)
                list(GET NCBI_COMPONENT_wxWidgets_LIBS 0 _lib)
                get_filename_component(_libdir ${_lib} DIRECTORY)
                set(NCBI_COMPONENT_wxWidgets_INCLUDE ${NCBI_COMPONENT_wxWidgets_INCLUDE}/wx-3.1 ${_libdir}/wx/include/gtk2-ansi-3.1 ${GTK2_INCLUDE_DIRS})
                set(NCBI_COMPONENT_wxWidgets_LIBS    ${NCBI_COMPONENT_wxWidgets_LIBS} ${NCBI_COMPONENT_FONTCONFIG_LIBS} ${GTK2_LIBRARIES} -lXxf86vm -lSM -lexpat)
                if(BUILD_SHARED_LIBS)
                    set(NCBI_COMPONENT_wxWidgets_DEFINES __WXGTK__  WXUSINGDLL wxDEBUG_LEVEL=0)
                else()
                    set(NCBI_COMPONENT_wxWidgets_DEFINES __WXGTK__ wxDEBUG_LEVEL=0)
                endif()
            endif()
        endif()
    endif()
endif()

##############################################################################
# GCRYPT
NCBI_define_component(GCRYPT gcrypt)
NCBI_define_component(GPG    gpg-error)
if(NCBI_COMPONENT_GCRYPT_FOUND)
    set(NCBI_COMPONENT_GCRYPT_INCLUDE ${NCBI_COMPONENT_GCRYPT_INCLUDE} ${NCBI_COMPONENT_GPG_INCLUDE})
    set(NCBI_COMPONENT_GCRYPT_LIBS    ${NCBI_COMPONENT_GCRYPT_LIBS}    ${NCBI_COMPONENT_GPG_LIBS})
endif()

#############################################################################
# XML
if(ON)
    NCBI_find_package(XML libxml-2.0)
endif()
if(NOT NCBI_COMPONENT_XML_FOUND)
    NCBI_define_component(XML xml2)
    if (NCBI_COMPONENT_XML_FOUND)
        set(NCBI_COMPONENT_XML_INCLUDE ${NCBI_COMPONENT_XML_INCLUDE} ${NCBI_COMPONENT_XML_INCLUDE}/libxml2)
    endif()
endif()

#############################################################################
# XSLT
if(ON)
    NCBI_find_package(XSLT libxslt)
endif()
if(NOT NCBI_COMPONENT_XSLT_FOUND)
    if (NCBI_COMPONENT_GCRYPT_FOUND)
        NCBI_define_component(XSLT xslt exslt xml2)
        if (NCBI_COMPONENT_XSLT_FOUND)
            set(NCBI_COMPONENT_XSLT_INCLUDE ${NCBI_COMPONENT_XSLT_INCLUDE} ${NCBI_COMPONENT_XSLT_INCLUDE}/libxml2)
            set(NCBI_COMPONENT_XSLT_LIBS ${NCBI_COMPONENT_XSLT_LIBS} ${NCBI_COMPONENT_GCRYPT_LIBS})
        endif()
    else()
        message("NOT FOUND XSLT: required component GCRYPT not found")
    endif()
endif()
if(NCBI_COMPONENT_XSLT_FOUND)
    set(NCBI_COMPONENT_XSLTPROCTOOL_FOUND YES)
    set(NCBI_XSLTPROCTOOL ${NCBI_ThirdParty_XSLT}/bin/xsltproc)
endif()

#############################################################################
# EXSLT
if(ON)
    NCBI_find_package(EXSLT libexslt)
endif()
if(NOT NCBI_COMPONENT_EXSLT_FOUND)
    if (NCBI_COMPONENT_GCRYPT_FOUND)
        NCBI_define_component(EXSLT xslt exslt xml2)
        if (NCBI_COMPONENT_EXSLT_FOUND)
            set(NCBI_COMPONENT_EXSLT_INCLUDE ${NCBI_COMPONENT_EXSLT_INCLUDE} ${NCBI_COMPONENT_EXSLT_INCLUDE}/libxml2)
            set(NCBI_COMPONENT_EXSLT_LIBS ${NCBI_COMPONENT_EXSLT_LIBS} ${NCBI_COMPONENT_GCRYPT_LIBS})
        endif()
    else()
        message("NOT FOUND EXSLT: required component GCRYPT not found")
    endif()
endif()

#############################################################################
# XLSXWRITER
NCBI_define_component(XLSXWRITER xlsxwriter)

#############################################################################
# LAPACK
if(ON)
    NCBI_find_package(LAPACK LAPACK)
    if(NCBI_COMPONENT_LAPACK_FOUND)
        set(LAPACK_FOUND YES)
    endif()
else()
    if(NOT NCBI_COMPONENT_LAPACK_DISABLED)
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
    else()
        message("DISABLED LAPACK")
    endif()
endif()

if(NCBI_COMPONENT_LAPACK_FOUND)
    check_include_file(lapacke.h HAVE_LAPACKE_H)
    check_include_file(lapacke/lapacke.h HAVE_LAPACKE_LAPACKE_H)
    check_include_file(Accelerate/Accelerate.h HAVE_ACCELERATE_ACCELERATE_H)
endif()

#############################################################################
# SAMTOOLS
if(NOT NCBI_COMPONENT_SAMTOOLS_DISABLED)
    find_external_library(samtools INCLUDES bam.h  LIBS bam HINTS "${NCBI_TOOLS_ROOT}/samtools")
    if(SAMTOOLS_FOUND)
        set(NCBI_COMPONENT_SAMTOOLS_FOUND YES)
        set(NCBI_COMPONENT_SAMTOOLS_INCLUDE ${SAMTOOLS_INCLUDE})
        set(NCBI_COMPONENT_SAMTOOLS_LIBS    ${SAMTOOLS_LIBS})
        list(APPEND NCBI_ALL_COMPONENTS SAMTOOLS)
    else()
        set(NCBI_COMPONENT_SAMTOOLS_FOUND NO)
    endif()
else()
    message("DISABLED SAMTOOLS")
endif()

#############################################################################
# FreeType
if(ON)
    NCBI_find_package(FreeType Freetype)
else()
    NCBI_find_library(FreeType freetype)
    if(NCBI_COMPONENT_FreeType_FOUND)
        set(NCBI_COMPONENT_FreeType_INCLUDE "/usr/include/freetype2")
    endif()
endif()

#############################################################################
# FTGL
if(ON)
    NCBI_find_package(FTGL ftgl)
endif()
if(NOT NCBI_COMPONENT_FTGL_FOUND)
    NCBI_define_component(FTGL ftgl)
endif()

#############################################################################
# GLEW
if(ON)
    NCBI_find_package(GLEW glew)
    if(NCBI_COMPONENT_GLEW_FOUND)
        get_filename_component(_incdir ${NCBI_COMPONENT_GLEW_INCLUDE} DIRECTORY)
        get_filename_component(_incGL ${NCBI_COMPONENT_GLEW_INCLUDE} NAME)
        if("${_incGL}" STREQUAL "GL")
            set(NCBI_COMPONENT_GLEW_INCLUDE ${_incdir})
        endif()
    endif()
endif()
if(NOT NCBI_COMPONENT_GLEW_FOUND)
    NCBI_define_component(GLEW GLEW)
endif()
if(NCBI_COMPONENT_GLEW_FOUND)
    if(BUILD_SHARED_LIBS)
        set(NCBI_COMPONENT_GLEW_DEFINES ${NCBI_COMPONENT_GLEW_DEFINES} GLEW_MX)
    else()
        set(NCBI_COMPONENT_GLEW_DEFINES ${NCBI_COMPONENT_GLEW_DEFINES} GLEW_MX GLEW_STATIC)
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
if(ON)
    NCBI_find_package(XERCES xerces-c)
endif()
if(NOT NCBI_COMPONENT_XERCES_FOUND)
    NCBI_define_component(XERCES xerces-c)
endif()

##############################################################################
# GRPC/PROTOBUF
set(NCBI_PROTOC_APP "${NCBI_ThirdParty_GRPC}/${CMAKE_BUILD_TYPE}${NCBI_PlatformBits}/bin/protoc")
set(NCBI_GRPC_PLUGIN "${NCBI_ThirdParty_GRPC}/${CMAKE_BUILD_TYPE}${NCBI_PlatformBits}/bin/grpc_cpp_plugin")

if(ON)
    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        NCBI_define_component(PROTOBUF protobufd)
    else()
        NCBI_find_package(PROTOBUF protobuf)
    endif()
    NCBI_find_package(GRPC grpc++)
    NCBI_define_component(Boring boringssl boringcrypto)
    if(NCBI_COMPONENT_GRPC_FOUND)
        set(NCBI_COMPONENT_GRPC_LIBS  ${NCBI_COMPONENT_GRPC_LIBS} ${NCBI_COMPONENT_Boring_LIBS})
    endif()
endif()
if(NOT NCBI_COMPONENT_GRPC_FOUND)
    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
      set(_suffix "d")
    else()
      set(_suffix "")
    endif()
    NCBI_define_component(PROTOBUF protobuf${_suffix})
    NCBI_define_component(GRPC 
        grpc++ grpc address_sorting upb cares gpr absl_bad_optional_access absl_str_format_internal
        absl_strings absl_strings_internal absl_base absl_spinlock_wait absl_dynamic_annotations
        absl_int128 absl_throw_delegate absl_raw_logging_internal absl_log_severity boringssl boringcrypto
        protobuf${_suffix})
endif()

#############################################################################
# XALAN
NCBI_define_component(XALAN xalan-c xalanMsg)

##############################################################################
# PERL

#does not work
#NCBI_find_package(PERL PerlLibs)
if(NOT NCBI_COMPONENT_PERL_DISABLED)
    find_package(PerlLibs)
    if (PERLLIBS_FOUND)
        set(NCBI_COMPONENT_PERL_FOUND   YES)
        set(NCBI_COMPONENT_PERL_INCLUDE ${PERL_INCLUDE_PATH})
        set(NCBI_COMPONENT_PERL_LIBS    ${PERL_LIBRARY})
        list(APPEND NCBI_ALL_COMPONENTS PERL)
    endif()
else()
    message("DISABLED PERL")
endif()

#############################################################################
# OpenSSL
if(ON)
    NCBI_find_package(OpenSSL OpenSSL)
else()
    NCBI_find_library(OpenSSL ssl crypto)
endif()

#############################################################################
# MSGSL  (Microsoft Guidelines Support Library)
NCBI_define_component(MSGSL)

#############################################################################
# SGE  (Sun Grid Engine)
if(NOT NCBI_COMPONENT_SGE_DISABLED)
    find_external_library(SGE INCLUDES drmaa.h LIBS drmaa
        INCLUDE_HINTS "${NCBI_ThirdParty_SGE}/include"
        LIBS_HINTS "${NCBI_ThirdParty_SGE}/ncbi-lib/lx-amd64/")
    if (SGE_FOUND)
        set(NCBI_COMPONENT_SGE_FOUND YES)
        set(NCBI_COMPONENT_SGE_INCLUDE ${SGE_INCLUDE})
        set(NCBI_COMPONENT_SGE_LIBS    ${SGE_LIBS})
        set(HAVE_LIBSGE 1)
        list(APPEND NCBI_ALL_COMPONENTS SGE)
    endif()
else()
    message("DISABLED SGE")
endif()

#############################################################################
# MONGOCXX
if(ON)
    NCBI_find_package(MONGOCXX libmongocxx)
endif()
if(NOT NCBI_COMPONENT_MONGOCXX_FOUND)
    NCBI_define_component(MONGOCXX mongocxx bsoncxx)
    if(NCBI_COMPONENT_MONGOCXX_FOUND)
        set(NCBI_COMPONENT_MONGOCXX_INCLUDE ${NCBI_COMPONENT_MONGOCXX_INCLUDE}/mongocxx/v_noabi ${NCBI_COMPONENT_MONGOCXX_INCLUDE}/bsoncxx/v_noabi)
        NCBI_define_component(MONGOC mongoc bson)
        if(NCBI_COMPONENT_MONGOC_FOUND)
            set(NCBI_COMPONENT_MONGOCXX_INCLUDE ${NCBI_COMPONENT_MONGOCXX_INCLUDE} ${NCBI_COMPONENT_MONGOC_INCLUDE})
            set(NCBI_COMPONENT_MONGOCXX_LIBS    ${NCBI_COMPONENT_MONGOCXX_LIBS}    ${NCBI_COMPONENT_MONGOC_LIBS})
        endif()
    endif()
endif()

#############################################################################
# LEVELDB
# only has cmake cfg
NCBI_define_component(LEVELDB leveldb)

#############################################################################
# WGMLST
if(DEFINED NCBI_COMPONENT_WGMLST_DISABLED AND NOT NCBI_COMPONENT_WGMLST_DISABLED)
    find_package(SKESA)
    if (WGMLST_FOUND)
        set(NCBI_COMPONENT_WGMLST_FOUND YES)
        set(NCBI_COMPONENT_WGMLST_INCLUDE ${WGMLST_INCLUDE_DIRS})
        set(NCBI_COMPONENT_WGMLST_LIBS    ${WGMLST_LIBPATH} ${WGMLST_LIBRARIES})
        list(APPEND NCBI_ALL_COMPONENTS WGMLST)
    endif()
else()
    message("DISABLED WGMLST")
endif()

#############################################################################
# GLPK
if(NOT NCBI_COMPONENT_GLPK_DISABLED)
    find_external_library(glpk INCLUDES glpk.h LIBS glpk
        HINTS "/usr/local/glpk/4.45")
    if(GLPK_FOUND)
        set(NCBI_COMPONENT_GLPK_FOUND YES)
        set(NCBI_COMPONENT_GLPK_INCLUDE ${GLPK_INCLUDE})
        set(NCBI_COMPONENT_GLPK_LIBS    ${GLPK_LIBS})
        list(APPEND NCBI_ALL_COMPONENTS GLPK)
    endif()
else()
    message("DISABLED GLPK")
endif()

#############################################################################
# UV
if(ON)
    NCBI_find_package(UV libuv)
endif()
if(NOT NCBI_COMPONENT_UV_FOUND)
    NCBI_define_component(UV uv)
endif()

#############################################################################
# NGHTTP2
if(ON)
    NCBI_find_package(NGHTTP2 libnghttp2)
endif()
if(NOT NCBI_COMPONENT_NGHTTP2_FOUND)
    NCBI_define_component(NGHTTP2 nghttp2)
endif()

#############################################################################
# GL2PS
NCBI_define_component(GL2PS gl2ps)

#############################################################################
# GMOCK
if(ON)
    NCBI_find_package(GMOCK gmock)
    NCBI_find_package(GTEST gtest)
    if(NCBI_COMPONENT_GMOCK_FOUND)
        set(NCBI_COMPONENT_GMOCK_INCLUDE ${NCBI_COMPONENT_GMOCK_INCLUDE} ${NCBI_COMPONENT_GTEST_INCLUDE})
        set(NCBI_COMPONENT_GMOCK_LIBS    ${NCBI_COMPONENT_GMOCK_LIBS}    ${NCBI_COMPONENT_GTEST_LIBS})
        set(NCBI_COMPONENT_GMOCK_DEFINES ${NCBI_COMPONENT_GMOCK_DEFINES} ${NCBI_COMPONENT_GTEST_DEFINES})
    endif()
endif()
if(NOT NCBI_COMPONENT_GMOCK_FOUND)
    NCBI_define_component(GMOCK gmock gtest)
endif()

#############################################################################
# CASSANDRA
if(OFF)
    NCBI_find_package(CASSANDRA cassandra)
endif()
if(NOT NCBI_COMPONENT_CASSANDRA_FOUND)
    NCBI_define_component(CASSANDRA cassandra)
endif()

#############################################################################
# H2O
if(ON)
    NCBI_find_package(H2O libh2o)
endif()
if(NOT NCBI_COMPONENT_H2O_FOUND)
    NCBI_define_component(H2O h2o)
    if(NCBI_COMPONENT_H2O_FOUND AND NCBI_COMPONENT_OpenSSL_FOUND)
        set(NCBI_COMPONENT_H2O_LIBS ${NCBI_COMPONENT_H2O_LIBS} ${NCBI_COMPONENT_OpenSSL_LIBS})
    endif()
endif()
