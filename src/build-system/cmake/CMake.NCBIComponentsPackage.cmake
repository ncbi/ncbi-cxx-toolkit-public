#############################################################################
# $Id$
#############################################################################

##
## NCBI CMake components description - download/build using Conan
##
##
## As a result, the following variables should be defined for component XXX
##  NCBI_COMPONENT_XXX_FOUND
##  NCBI_COMPONENT_XXX_INCLUDE
##  NCBI_COMPONENT_XXX_DEFINES
##  NCBI_COMPONENT_XXX_LIBS
##  HAVE_LIBXXX
##  HAVE_XXX


#set(NCBI_TRACE_ALLCOMPONENTS ON)
#set(NCBI_TRACE_COMPONENT_BZ2 ON)

if(COMMAND conan_define_targets)
    set(__silent ${CONAN_CMAKE_SILENT_OUTPUT})	 
    set(CONAN_CMAKE_SILENT_OUTPUT TRUE)	 
    conan_define_targets()
    set(CONAN_CMAKE_SILENT_OUTPUT ${__silent})
endif()
list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})
if(EXISTS "${CMAKE_BINARY_DIR}/${NCBI_DIRNAME_CONANGEN}")
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}/${NCBI_DIRNAME_CONANGEN}")
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/${NCBI_DIRNAME_CONANGEN}")
endif()


#############################################################################
#############################################################################

if(NOT COMMAND conan_define_targets)
    NCBI_define_Pkgcomponent(NAME OpenSSL PACKAGE openssl FIND OpenSSL)
endif()

#############################################################################
# NCBICRYPT
NCBI_define_Pkgcomponent(NAME NCBICRYPT PACKAGE ncbicrypt FIND ncbicrypt)

#############################################################################
# BACKWARD, UNWIND
NCBI_define_Pkgcomponent(NAME BACKWARD PACKAGE backward-cpp REQUIRES libdwarf FIND Backward)
list(REMOVE_ITEM NCBI_ALL_COMPONENTS BACKWARD)
if(NCBI_COMPONENT_BACKWARD_FOUND)
    set(HAVE_LIBBACKWARD_CPP YES)
    set(HAVE_BACKWARD_HPP YES)
endif()
NCBI_define_Pkgcomponent(NAME UNWIND PACKAGE libunwind REQUIRES xz_utils;zlib FIND libunwind)
#list(REMOVE_ITEM NCBI_ALL_COMPONENTS UNWIND)

##############################################################################
# CURL
NCBI_define_Pkgcomponent(NAME CURL PACKAGE CURL FIND CURL)

#############################################################################
# Iconv
if(DEFINED CONAN_LIBICONV_ROOT)
    set(ICONV_LIBS ${CONAN_LIBS_LIBICONV})
    set(HAVE_LIBICONV 1)
    set(NCBI_REQUIRE_Iconv_FOUND YES)
else()
    if(NOT TARGET Iconv::Iconv)
        NCBI_define_Pkgcomponent(NAME Iconv PACKAGE LIBICONV FIND Iconv)
    endif()
    if(TARGET Iconv::Iconv)
        set(ICONV_LIBS Iconv::Iconv)
        set(HAVE_LIBICONV 1)
        set(NCBI_REQUIRE_Iconv_FOUND YES)
    endif()
endif()

#############################################################################
# LMDB
NCBI_define_Pkgcomponent(NAME LMDB PACKAGE lmdb FIND lmdb)
if(NOT NCBI_COMPONENT_LMDB_FOUND)
    set(NCBI_COMPONENT_LMDB_FOUND ${NCBI_COMPONENT_LocalLMDB_FOUND})
    set(NCBI_COMPONENT_LMDB_INCLUDE ${NCBI_COMPONENT_LocalLMDB_INCLUDE})
    set(NCBI_COMPONENT_LMDB_NCBILIB ${NCBI_COMPONENT_LocalLMDB_NCBILIB})
    set(HAVE_LIBLMDB ${NCBI_COMPONENT_LMDB_FOUND})
endif()

#############################################################################
# PCRE
NCBI_define_Pkgcomponent(NAME PCRE PACKAGE pcre REQUIRES bzip2;zlib FIND PCRE)
NCBI_define_Pkgcomponent(NAME PCRE2 PACKAGE pcre2 REQUIRES bzip2;zlib FIND PCRE2)
if(NOT NCBI_COMPONENT_PCRE_FOUND AND NOT NCBI_COMPONENT_PCRE2_FOUND)
  if(EXISTS ${NCBITK_INC_ROOT}/util/regexp/pcre.h)
    set(NCBI_COMPONENT_PCRE_FOUND ${NCBI_COMPONENT_LocalPCRE_FOUND})
    set(NCBI_COMPONENT_PCRE_INCLUDE ${NCBI_COMPONENT_LocalPCRE_INCLUDE})
    set(NCBI_COMPONENT_PCRE_NCBILIB ${NCBI_COMPONENT_LocalPCRE_NCBILIB})
    set(HAVE_LIBPCRE ${NCBI_COMPONENT_PCRE_FOUND})
  else()
    set(NCBI_COMPONENT_PCRE2_FOUND ${NCBI_COMPONENT_LocalPCRE_FOUND})
    set(NCBI_COMPONENT_PCRE2_INCLUDE ${NCBI_COMPONENT_LocalPCRE_INCLUDE})
    set(NCBI_COMPONENT_PCRE2_NCBILIB ${NCBI_COMPONENT_LocalPCRE_NCBILIB})
    set(HAVE_LIBPCRE2 ${NCBI_COMPONENT_PCRE2_FOUND})
  endif()
endif()

#############################################################################
# Z
NCBI_define_Pkgcomponent(NAME Z PACKAGE zlib FIND ZLIB)
if(NOT NCBI_COMPONENT_Z_FOUND)
    set(NCBI_COMPONENT_Z_FOUND ${NCBI_COMPONENT_LocalZ_FOUND})
    set(NCBI_COMPONENT_Z_INCLUDE ${NCBI_COMPONENT_LocalZ_INCLUDE})
    set(NCBI_COMPONENT_Z_NCBILIB ${NCBI_COMPONENT_LocalZ_NCBILIB})
    set(HAVE_LIBZ ${NCBI_COMPONENT_Z_FOUND})
endif()

#############################################################################
# BZ2
NCBI_define_Pkgcomponent(NAME BZ2 PACKAGE bzip2 FIND BZip2)
if(NOT NCBI_COMPONENT_BZ2_FOUND)
    set(NCBI_COMPONENT_BZ2_FOUND ${NCBI_COMPONENT_LocalBZ2_FOUND})
    set(NCBI_COMPONENT_BZ2_INCLUDE ${NCBI_COMPONENT_LocalBZ2_INCLUDE})
    set(NCBI_COMPONENT_BZ2_NCBILIB ${NCBI_COMPONENT_LocalBZ2_NCBILIB})
    set(HAVE_LIBBZ2 ${NCBI_COMPONENT_BZ2_FOUND})
endif()

#############################################################################
# LZO
NCBI_define_Pkgcomponent(NAME LZO PACKAGE lzo FIND lzo)

#############################################################################
# ZSTD
NCBI_define_Pkgcomponent(NAME ZSTD PACKAGE zstd FIND zstd)
if(NCBI_COMPONENT_ZSTD_FOUND AND
    (DEFINED NCBI_COMPONENT_ZSTD_VERSION AND "${NCBI_COMPONENT_ZSTD_VERSION}" VERSION_LESS "1.4"))
    message("ZSTD: Version requirement not met (required at least v1.4)")
    set(NCBI_COMPONENT_ZSTD_FOUND NO)
    set(HAVE_LIBZSTD 0)
endif()

#############################################################################
# Boost
include(${NCBI_TREE_CMAKECFG}/CMakeChecks.boost.cmake)
if(NCBI_COMPONENT_Boost_FOUND)
    NCBI_map_imported_config(Boost ${NCBI_COMPONENT_Boost_LIBS})
endif()

#############################################################################
# JPEG
NCBI_define_Pkgcomponent(NAME JPEG PACKAGE libjpeg FIND JPEG)

#############################################################################
# PNG
NCBI_define_Pkgcomponent(NAME PNG PACKAGE libpng REQUIRES zlib FIND PNG)

#############################################################################
# GIF
NCBI_define_Pkgcomponent(NAME GIF PACKAGE giflib FIND GIF)

#############################################################################
# TIFF
NCBI_define_Pkgcomponent(NAME TIFF PACKAGE libtiff REQUIRES zlib;libdeflate;xz_utils;libjpeg;jbig;zstd;libwebp FIND TIFF)

#############################################################################
# FASTCGI
NCBI_define_Pkgcomponent(NAME FASTCGI PACKAGE ncbi-fastcgi FIND ncbi-fastcgi)

#############################################################################
# FASTCGIPP
NCBI_define_Pkgcomponent(NAME FASTCGIPP PACKAGE ncbi-fastcgipp FIND ncbi-fastcgipp)

#############################################################################
# SQLITE3
NCBI_define_Pkgcomponent(NAME SQLITE3 PACKAGE sqlite3 FIND SQLite3)
if(NCBI_COMPONENT_SQLITE3_FOUND)
#    check_symbol_exists(sqlite3_unlock_notify ${NCBI_COMPONENT_SQLITE3_INCLUDE}/sqlite3.h HAVE_SQLITE3_UNLOCK_NOTIFY)
#    check_include_file(sqlite3async.h HAVE_SQLITE3ASYNC_H -I${NCBI_COMPONENT_SQLITE3_INCLUDE})
    set(HAVE_SQLITE3_UNLOCK_NOTIFY YES)
    set(HAVE_SQLITE3ASYNC_H NO)
endif()

#############################################################################
# BerkeleyDB
NCBI_define_Pkgcomponent(NAME BerkeleyDB PACKAGE libdb FIND libdb)
if(NCBI_COMPONENT_BerkeleyDB_FOUND)
    set(HAVE_BERKELEY_DB 1)
    set(HAVE_BDB         1)
    set(HAVE_BDB_CACHE   1)
endif()

#############################################################################
# XML
NCBI_define_Pkgcomponent(NAME XML PACKAGE libxml2 REQUIRES zlib;libiconv FIND LibXml2)
if (NCBI_COMPONENT_XML_FOUND)
    if(CMAKE_COMPILER_IS_GNUCC)
        set(NCBI_COMPONENT_XML_LIBS -Wl,--no-as-needed ${NCBI_COMPONENT_XML_LIBS})
    endif()
endif()

#############################################################################
# XSLT
NCBI_define_Pkgcomponent(NAME XSLT PACKAGE libxslt REQUIRES libxml2;zlib;libiconv FIND LibXslt)
if(NOT DEFINED NCBI_XSLTPROCTOOL)
    if(DEFINED CONAN_BIN_DIRS_LIBXSLT)
        set(NCBI_XSLTPROCTOOL "${CONAN_BIN_DIRS_LIBXSLT}/xsltproc${CMAKE_EXECUTABLE_SUFFIX}")
    endif()
    if(EXISTS "${NCBI_XSLTPROCTOOL}")
        set(NCBI_REQUIRE_XSLTPROCTOOL_FOUND YES)
    endif()
endif()
if(NCBI_TRACE_COMPONENT_XSLT OR NCBI_TRACE_ALLCOMPONENTS)
    message("NCBI_XSLTPROCTOOL = ${NCBI_XSLTPROCTOOL}")
endif()

#############################################################################
# EXSLT
NCBI_define_Pkgcomponent(NAME EXSLT PACKAGE libxslt REQUIRES libxml2;zlib;libiconv FIND LibXslt)
if(NCBI_COMPONENT_EXSLT_FOUND AND TARGET LibXslt::LibExslt)
    set(NCBI_COMPONENT_EXSLT_LIBS LibXslt::LibExslt ${NCBI_COMPONENT_EXSLT_LIBS})
    NCBI_map_imported_config(EXSLT ${NCBI_COMPONENT_EXSLT_LIBS})
endif()

#############################################################################
# UV
NCBI_define_Pkgcomponent(NAME UV PACKAGE libuv FIND libuv)

#############################################################################
# NGHTTP2
NCBI_define_Pkgcomponent(NAME NGHTTP2 PACKAGE libnghttp2 REQUIRES zlib FIND libnghttp2)

##############################################################################
# GRPC/PROTOBUF
if(NOT APPLE)
    NCBI_util_disable_find_use_path()
endif()
NCBI_define_Pkgcomponent(NAME PROTOBUF PACKAGE protobuf REQUIRES zlib FIND Protobuf)
if(NOT NCBI_PROTOC_APP)
    if(DEFINED CONAN_BIN_DIRS_PROTOBUF)
        set(NCBI_PROTOC_APP "${CONAN_BIN_DIRS_PROTOBUF}/protoc${CMAKE_EXECUTABLE_SUFFIX}")
    elseif(TARGET protobuf::protoc)
        foreach( _type IN ITEMS "" "_RELEASE" "_DEBUG")
            get_property(NCBI_PROTOC_APP TARGET protobuf::protoc PROPERTY IMPORTED_LOCATION${_type})
            if(EXISTS "${NCBI_PROTOC_APP}")
                break()
            endif()
        endforeach()
    endif()
endif()
if(NCBI_TRACE_COMPONENT_PROTOBUF OR NCBI_TRACE_ALLCOMPONENTS)
    message("NCBI_PROTOC_APP = ${NCBI_PROTOC_APP}")
endif()

NCBI_define_Pkgcomponent(NAME GRPC PACKAGE grpc REQUIRES abseil;c-ares;openssl;protobuf;re2;zlib FIND gRPC)
if(NOT NCBI_GRPC_PLUGIN)
    if(DEFINED CONAN_BIN_DIRS_GRPC)
        set(NCBI_GRPC_PLUGIN "${CONAN_BIN_DIRS_GRPC}/grpc_cpp_plugin${CMAKE_EXECUTABLE_SUFFIX}")
    elseif(TARGET gRPC::grpc_cpp_plugin)
        foreach( _type IN ITEMS "" "_RELEASE" "_DEBUG")
            get_property(NCBI_GRPC_PLUGIN TARGET gRPC::grpc_cpp_plugin PROPERTY IMPORTED_LOCATION${_type})
            if(EXISTS "${NCBI_GRPC_PLUGIN}")
                break()
            endif()
        endforeach()
    endif()
endif()
if(NCBI_TRACE_COMPONENT_GRPC OR NCBI_TRACE_ALLCOMPONENTS)
    message("NCBI_GRPC_PLUGIN = ${NCBI_GRPC_PLUGIN}")
endif()
if(NOT APPLE)
    NCBI_util_enable_find_use_path()
endif()

#############################################################################
# CASSANDRA
NCBI_define_Pkgcomponent(NAME CASSANDRA PACKAGE cassandra-cpp-driver REQUIRES http_parser;libuv;minizip;openssl;rapidjson;zlib FIND cassandra-cpp-driver)

#############################################################################
# MySQL
if(NCBI_PTBCFG_PACKAGED)
    NCBI_define_Pkgcomponent(NAME MySQL PACKAGE libmysqlclient REQUIRES lz4;openssl;zlib;zstd FIND MySQL)
else()
    NCBI_define_Pkgcomponent(NAME MySQL PACKAGE libmysqlclient REQUIRES lz4;openssl;zlib;zstd FIND libmysqlclient)
endif()

#############################################################################
# VDB
NCBI_define_Pkgcomponent(NAME VDB PACKAGE ncbi-vdb FIND ncbi-vdb)
if(NCBI_COMPONENT_VDB_FOUND)
    set(HAVE_NCBI_VDB 1)
endif()

#############################################################################
# JAEGER
NCBI_define_Pkgcomponent(NAME JAEGER PACKAGE jaegertracing FIND jaegertracing)

#############################################################################
# OPENTELEMETRY
NCBI_define_Pkgcomponent(NAME OPENTELEMETRY PACKAGE opentelemetry-cpp
  FIND opentelemetry-cpp)

#############################################################################
# AWS_SDK
NCBI_define_Pkgcomponent(NAME AWS_SDK PACKAGE AWSSDK FIND AWSSDK)

#############################################################################
# wxWidgets
NCBI_define_Pkgcomponent(NAME wxWidgets PACKAGE wxWidgets FIND wxWidgets)

