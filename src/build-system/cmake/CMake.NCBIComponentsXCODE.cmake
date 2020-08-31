#############################################################################
# $Id$
#############################################################################

##
## NCBI CMake components description - XCODE
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
if(XCODE)
    set(NCBI_REQUIRE_XCODE_FOUND YES)
    list(APPEND NCBI_ALL_REQUIRES XCODE)
endif()
#to debug
#set(NCBI_TRACE_COMPONENT_JPEG ON)
#############################################################################
# common settings
set(NCBI_TOOLS_ROOT $ENV{NCBI})
set(NCBI_OPT_ROOT  /opt/X11)
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

set(FOUNDATION_LIBS "-framework foundation")

#############################################################################
# Kerberos 5
set(KRB5_LIBS "-framework Kerberos" -liconv)

############################################################################
set(NCBI_ThirdParty_BACKWARD   ${NCBI_TOOLS_ROOT}/backward-cpp-1.3.20180206-44ae960 CACHE PATH "BACKWARD root")
set(NCBI_ThirdParty_TLS        ${NCBI_TOOLS_ROOT}/gnutls-3.4.0 CACHE PATH "TLS root")
set(NCBI_ThirdParty_LMDB       ${NCBI_TOOLS_ROOT}/lmdb-0.9.18 CACHE PATH "LMDB root")
set(NCBI_ThirdParty_LZO        ${NCBI_TOOLS_ROOT}/lzo-2.05 CACHE PATH "LZO root")
set(NCBI_ThirdParty_SQLITE3    ${NCBI_TOOLS_ROOT}/sqlite-3.26.0-ncbi1 CACHE PATH "SQLITE2 root")
set(NCBI_ThirdParty_Boost      ${NCBI_TOOLS_ROOT}/boost-1.62.0-ncbi1 CACHE PATH "Boost root")
set(NCBI_ThirdParty_BerkeleyDB ${NCBI_TOOLS_ROOT}/BerkeleyDB CACHE PATH "BerkeleyDB root")
#set(NCBI_ThirdParty_VDB        "/Volumes/trace_software/vdb/vdb-versions/2.10.8")
set(NCBI_ThirdParty_VDB        "/net/snowman/vol/projects/trace_software/vdb/vdb-versions/2.10.8" CACHE PATH "VDB root")
set(NCBI_ThirdParty_VDB_ARCH x86_64)
set(NCBI_ThirdParty_XML        ${NCBI_TOOLS_ROOT}/libxml-2.7.8 CACHE PATH "XML root")
set(NCBI_ThirdParty_XSLT       ${NCBI_TOOLS_ROOT}/libxml-2.7.8 CACHE PATH "XSLT root")
set(NCBI_ThirdParty_EXSLT      ${NCBI_ThirdParty_XSLT})
set(NCBI_ThirdParty_FTGL      ${NCBI_TOOLS_ROOT}/ftgl-2.1.3-rc5 CACHE PATH "FTGL root")
set(NCBI_ThirdParty_GLEW      ${NCBI_TOOLS_ROOT}/glew-1.5.8 CACHE PATH "GLEW root")
set(NCBI_ThirdParty_GRPC      ${NCBI_TOOLS_ROOT}/grpc-1.28.1-ncbi1 CACHE PATH "GRPC root")
set(NCBI_ThirdParty_PROTOBUF  ${NCBI_TOOLS_ROOT}/grpc-1.28.1-ncbi1 CACHE PATH "PROTOBUF root")
set(NCBI_ThirdParty_wxWidgets ${NCBI_TOOLS_ROOT}/wxWidgets-3.1.3-ncbi1 CACHE PATH "wxWidgets root")
set(NCBI_ThirdParty_UV        ${NCBI_TOOLS_ROOT}/libuv-1.35.0 CACHE PATH "UV root")
set(NCBI_ThirdParty_NGHTTP2   ${NCBI_TOOLS_ROOT}/nghttp2-1.40.0 CACHE PATH "NGHTTP2 root")
set(NCBI_ThirdParty_GL2PS     ${NCBI_TOOLS_ROOT}/gl2ps-1.4.0 CACHE PATH "GL2PS root")
set(NCBI_ThirdParty_Nettle    ${NCBI_TOOLS_ROOT}/nettle-3.1.1)
set(NCBI_ThirdParty_Hogweed   ${NCBI_TOOLS_ROOT}/nettle-3.1.1)
set(NCBI_ThirdParty_GMP       ${NCBI_TOOLS_ROOT}/gmp-6.0.0a)

#############################################################################
#############################################################################

#############################################################################
# NCBI_C
set(NCBI_COMPONENT_NCBI_C_FOUND NO)

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
else(NOT NCBI_COMPONENT_BACKWARD_DISABLED)
    message("DISABLED BACKWARD")
endif(NOT NCBI_COMPONENT_BACKWARD_DISABLED)

##############################################################################
# CURL
NCBI_find_package(CURL CURL)

#############################################################################
#LMDB
NCBI_define_component(LMDB lmdb)
if(NOT NCBI_COMPONENT_LMDB_FOUND)
  set(NCBI_COMPONENT_LMDB_FOUND ${NCBI_COMPONENT_LocalLMDB_FOUND})
  set(NCBI_COMPONENT_LMDB_INCLUDE ${NCBI_COMPONENT_LocalLMDB_INCLUDE})
  set(NCBI_COMPONENT_LMDB_NCBILIB ${NCBI_COMPONENT_LocalLMDB_NCBILIB})
endif()

#############################################################################
# PCRE
#NCBI_find_library(PCRE pcre)
if(NOT NCBI_COMPONENT_PCRE_FOUND)
  set(NCBI_COMPONENT_PCRE_FOUND ${NCBI_COMPONENT_LocalPCRE_FOUND})
  set(NCBI_COMPONENT_PCRE_INCLUDE ${NCBI_COMPONENT_LocalPCRE_INCLUDE})
  set(NCBI_COMPONENT_PCRE_NCBILIB ${NCBI_COMPONENT_LocalPCRE_NCBILIB})
  list(APPEND NCBI_ALL_COMPONENTS PCRE)
endif()

#############################################################################
# Z
if(ON)
    NCBI_find_package(Z ZLIB)
else()
    set(NCBI_COMPONENT_Z_FOUND YES)
    set(NCBI_COMPONENT_Z_LIBS -lz)
    list(APPEND NCBI_ALL_COMPONENTS Z)
endif()

#############################################################################
#BZ2
if(ON)
    NCBI_find_package(BZ2 BZip2)
else()
    set(NCBI_COMPONENT_BZ2_FOUND YES)
    set(NCBI_COMPONENT_BZ2_LIBS -lbz2)
    list(APPEND NCBI_ALL_COMPONENTS BZ2)
endif()

#############################################################################
# LZO
NCBI_define_component(LZO lzo2)

#############################################################################
# Boost.Test.Included
if (EXISTS ${NCBI_ThirdParty_Boost}/include)
  message(STATUS "Found Boost.Test.Included: ${NCBI_ThirdParty_Boost}")
  set(NCBI_COMPONENT_Boost.Test.Included_FOUND YES)
  set(NCBI_COMPONENT_Boost.Test.Included_INCLUDE ${NCBI_ThirdParty_Boost}/include)
  list(APPEND NCBI_ALL_COMPONENTS Boost.Test.Included)
else()
  message("Component Boost.Test.Included ERROR: ${NCBI_ThirdParty_Boost}/include not found")
  set(NCBI_COMPONENT_Boost.Test.Included_FOUND NO)
endif()

#############################################################################
# Boost.Test
NCBI_define_component(Boost.Test boost_unit_test_framework-clang-darwin)

#############################################################################
# Boost.Spirit
NCBI_define_component(Boost.Spirit boost_thread-mt)

#############################################################################
# JPEG
NCBI_find_package(JPEG JPEG)

#############################################################################
# PNG
NCBI_find_package(PNG PNG)

#############################################################################
# GIF
#NCBI_find_package(GIF GIF)
#set(NCBI_COMPONENT_GIF_FOUND YES)
#list(APPEND NCBI_ALL_COMPONENTS GIF)

#############################################################################
# TIFF
NCBI_find_package(TIFF TIFF)

#############################################################################
# TLS
if (EXISTS ${NCBI_ThirdParty_TLS}/include)
  message(STATUS "Found TLS: ${NCBI_ThirdParty_TLS}")
  set(NCBI_COMPONENT_TLS_FOUND YES)
  set(NCBI_COMPONENT_TLS_INCLUDE ${NCBI_ThirdParty_TLS}/include)
  list(APPEND NCBI_ALL_COMPONENTS TLS)
else()
  message("Component TLS ERROR: ${NCBI_ThirdParty_TLS}/include not found")
  set(NCBI_COMPONENT_TLS_FOUND NO)
endif()

#############################################################################
# FASTCGI
set(NCBI_COMPONENT_FASTCGI_FOUND NO)

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
#BerkeleyDB
NCBI_define_component(BerkeleyDB db)
if(NCBI_COMPONENT_BerkeleyDB_FOUND)
  set(HAVE_BERKELEY_DB 1)
  set(HAVE_BDB         1)
  set(HAVE_BDB_CACHE   1)
endif()

#############################################################################
# ODBC
set(NCBI_COMPONENT_ODBC_FOUND NO)
set(ODBC_INCLUDE  ${NCBI_INC_ROOT}/dbapi/driver/odbc/unix_odbc 
                  ${NCBI_INC_ROOT}/dbapi/driver/odbc/unix_odbc)
set(NCBI_COMPONENT_ODBC_INCLUDE ${ODBC_INCLUDE})
set(HAVE_ODBC 0)
set(HAVE_ODBCSS_H 0)

#############################################################################
# MySQL
set(NCBI_COMPONENT_MySQL_FOUND NO)

#############################################################################
# Sybase
set(NCBI_COMPONENT_Sybase_FOUND NO)

#############################################################################
# PYTHON
set(NCBI_COMPONENT_PYTHON_FOUND NO)

#############################################################################
# VDB
if(NOT NCBI_COMPONENT_VDB_DISABLED)
    set(NCBI_COMPONENT_VDB_INCLUDE
        ${NCBI_ThirdParty_VDB}/interfaces
        ${NCBI_ThirdParty_VDB}/interfaces/cc/gcc/${NCBI_ThirdParty_VDB_ARCH}
        ${NCBI_ThirdParty_VDB}/interfaces/cc/gcc
        ${NCBI_ThirdParty_VDB}/interfaces/os/mac
        ${NCBI_ThirdParty_VDB}/interfaces/os/unix)
    set(NCBI_COMPONENT_VDB_LIBS
        ${NCBI_ThirdParty_VDB}/mac/release/${NCBI_ThirdParty_VDB_ARCH}/lib/libncbi-vdb.a)

    set(_found YES)
    foreach(_inc IN LISTS NCBI_COMPONENT_VDB_INCLUDE NCBI_COMPONENT_VDB_LIBS)
        if(NOT EXISTS ${_inc})
            message("Component VDB ERROR: ${_inc} not found")
            set(_found NO)
        endif()
    endforeach()
    if(_found)
        message(STATUS "Found VDB: ${NCBI_ThirdParty_VDB}")
        set(NCBI_COMPONENT_VDB_FOUND YES)
        set(HAVE_NCBI_VDB 1)
        list(APPEND NCBI_ALL_COMPONENTS VDB)
    else()
        set(NCBI_COMPONENT_VDB_FOUND NO)
        unset(NCBI_COMPONENT_VDB_INCLUDE)
        unset(NCBI_COMPONENT_VDB_LIBS)
    endif()
else()
    message("DISABLED VDB")
endif()

#############################################################################
# wxWidgets
set(_wx_ver 3.1)
NCBI_define_component(wxWidgets
    wx_osx_cocoa_gl-${_wx_ver}
    wx_osx_cocoa_richtext-${_wx_ver}
    wx_osx_cocoa_aui-${_wx_ver}
    wx_osx_cocoa_propgrid-${_wx_ver}
    wx_osx_cocoa_xrc-${_wx_ver}
    wx_osx_cocoa_qa-${_wx_ver}
    wx_osx_cocoa_html-${_wx_ver}
    wx_osx_cocoa_adv-${_wx_ver}
    wx_osx_cocoa_core-${_wx_ver}
    wx_base_xml-${_wx_ver}
    wx_base_net-${_wx_ver}
    wx_base-${_wx_ver}
)
if(NCBI_COMPONENT_wxWidgets_FOUND)
    list(GET NCBI_COMPONENT_wxWidgets_LIBS 0 _lib)
    get_filename_component(_libdir ${_lib} DIRECTORY)
    set(NCBI_COMPONENT_wxWidgets_INCLUDE ${NCBI_COMPONENT_wxWidgets_INCLUDE}/wx-${_wx_ver} ${_libdir}/wx/include/osx_cocoa-ansi-${_wx_ver})
    set(NCBI_COMPONENT_wxWidgets_LIBS    ${NCBI_COMPONENT_wxWidgets_LIBS}  "-framework Cocoa")
    set(NCBI_COMPONENT_wxWidgets_DEFINES __WXMAC__ __WXOSX__ __WXOSX_COCOA__ wxDEBUG_LEVEL=0)
endif()

#############################################################################
# XML
if(ON)
    NCBI_find_package(XML libxml-2.0)
endif()
if(NOT NCBI_COMPONENT_XML_FOUND)
    NCBI_define_component(XML xml2)
    if(NCBI_COMPONENT_XML_FOUND)
        set(NCBI_COMPONENT_XML_INCLUDE ${NCBI_ThirdParty_XML}/include/libxml2)
        set(NCBI_COMPONENT_XML_LIBS ${NCBI_COMPONENT_XML_LIBS} -liconv)
    endif()
endif()

#############################################################################
# XSLT
if(ON)
    NCBI_find_package(XSLT libxslt)
endif()
if(NOT NCBI_COMPONENT_XSLT_FOUND)
    NCBI_define_component(XSLT exslt xslt)
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
    NCBI_define_component(EXSLT exslt)
endif()

#############################################################################
# LAPACK
if(ON)
    NCBI_find_package(LAPACK LAPACK)
    if(NCBI_COMPONENT_LAPACK_FOUND)
        set(LAPACK_FOUND YES)
    endif()
else()
    if(NOT NCBI_COMPONENT_LAPACK_DISABLED)
        set(NCBI_COMPONENT_LAPACK_FOUND YES)
        set(NCBI_COMPONENT_LAPACK_LIBS -llapack)
        list(APPEND NCBI_ALL_COMPONENTS LAPACK)
    else()
        message("DISABLED LAPACK")
    endif()
endif()

#############################################################################
# FreeType
if(ON)
    NCBI_find_package(FreeType Freetype)
else()
    NCBI_define_component(FreeType freetype)
    if(NCBI_COMPONENT_FreeType_FOUND)
        set(NCBI_COMPONENT_FreeType_INCLUDE ${NCBI_COMPONENT_FreeType_INCLUDE} ${NCBI_COMPONENT_FreeType_INCLUDE}/freetype2)
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

#############################################################################
# OpenGL
if(OFF)
    NCBI_find_package(OpenGL OpenGL)
else()
    if(NOT NCBI_COMPONENT_OpenGL_DISABLED)
        set(NCBI_COMPONENT_OpenGL_FOUND YES)
        set(NCBI_COMPONENT_OpenGL_LIBS "-framework AGL -framework OpenGL -framework Metal -framework MetalKit")
        list(APPEND NCBI_ALL_COMPONENTS OpenGL)
    else()
        message("DISABLED OpenGL")
    endif()
endif()

##############################################################################
# GRPC/PROTOBUF
set(NCBI_PROTOC_APP "${NCBI_ThirdParty_GRPC}/bin/protoc")
set(NCBI_GRPC_PLUGIN "${NCBI_ThirdParty_GRPC}/bin/grpc_cpp_plugin")

if(ON)
    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        NCBI_define_component(PROTOBUF protobufd)
    else()
        NCBI_find_package(PROTOBUF protobuf)
    endif()
    NCBI_find_package(GRPC grpc++)
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
# Nettle
if(OFF)
if(ON)
    NCBI_find_package(Nettle nettle)
    NCBI_find_package(Hogweed hogweed)
    if(NCBI_COMPONENT_Nettle_FOUND)
        set(NCBI_COMPONENT_Nettle_LIBS  ${NCBI_COMPONENT_Nettle_LIBS} ${NCBI_COMPONENT_Hogweed_LIBS})
    endif()
else()
    NCBI_define_component(Nettle nettle hogweed)
endif()
endif()

#############################################################################
# GMP
#NCBI_define_component(GMP gmp)
