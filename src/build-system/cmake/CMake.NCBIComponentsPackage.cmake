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
set(__silent ${CONAN_CMAKE_SILENT_OUTPUT})
set(CONAN_CMAKE_SILENT_OUTPUT TRUE)
conan_define_targets()
set(CONAN_CMAKE_SILENT_OUTPUT ${__silent})

include(CheckIncludeFile)
include(CheckSymbolExists)
#############################################################################
function(NCBI_define_Pkgcomponent)
    cmake_parse_arguments(DC "" "NAME;PACKAGE" "REQUIRES" ${ARGN})

    if("${DC_NAME}" STREQUAL "")
        message(FATAL_ERROR "No component name")
    endif()
    if("${DC_PACKAGE}" STREQUAL "")
        message(FATAL_ERROR "No package name")
    endif()
    if(WIN32)
        set(_prefix "")
        set(_suffixes ${CMAKE_STATIC_LIBRARY_SUFFIX})
    else()
        set(_prefix lib)
        if(NCBI_PTBCFG_COMPONENT_StaticComponents)
            set(_suffixes ${CMAKE_STATIC_LIBRARY_SUFFIX} ${CMAKE_SHARED_LIBRARY_SUFFIX})
        else()
            if(BUILD_SHARED_LIBS OR TRUE)
                set(_suffixes ${CMAKE_SHARED_LIBRARY_SUFFIX} ${CMAKE_STATIC_LIBRARY_SUFFIX})
            else()
                set(_suffixes ${CMAKE_STATIC_LIBRARY_SUFFIX} ${CMAKE_SHARED_LIBRARY_SUFFIX})
            endif()
        endif()
    endif()
    string(TOUPPER ${DC_PACKAGE} _UPPACKAGE)
    set(DC_REQUIRES ${DC_PACKAGE} ${DC_REQUIRES})

    set(NCBI_COMPONENT_${DC_NAME}_FOUND NO PARENT_SCOPE)
    if(NCBI_COMPONENT_${DC_NAME}_DISABLED)
        message("DISABLED ${DC_NAME}")
    elseif(DEFINED CONAN_${_UPPACKAGE}_ROOT)
        if(NOT NCBI_PTBCFG_PACKAGED)
            message(STATUS "Found ${DC_NAME}: ${CONAN_${_UPPACKAGE}_ROOT}")
        endif()
        set(NCBI_COMPONENT_${DC_NAME}_FOUND YES PARENT_SCOPE)
        set(_include ${CONAN_INCLUDE_DIRS_${_UPPACKAGE}})
        set(_defines ${CONAN_DEFINES_${_UPPACKAGE}})
        set(NCBI_COMPONENT_${DC_NAME}_INCLUDE ${_include} PARENT_SCOPE)
        set(NCBI_COMPONENT_${DC_NAME}_DEFINES ${_defines} PARENT_SCOPE)

        set(_all_libs "")
        foreach(_package IN LISTS DC_REQUIRES)
            string(TOUPPER ${_package} _UPPACKAGE)
            if(DEFINED CONAN_${_UPPACKAGE}_ROOT)
                if(TARGET CONAN_PKG::${_package})
                    list(APPEND _all_libs CONAN_PKG::${_package})
                else()
                    if(NOT "${CONAN_LIB_DIRS_${_UPPACKAGE}}" STREQUAL "" AND NOT "${CONAN_LIBS_${_UPPACKAGE}}" STREQUAL "")
                        foreach(_lib IN LISTS CONAN_LIBS_${_UPPACKAGE})
                            set(_this_found NO)
                            foreach(_dir IN LISTS CONAN_LIB_DIRS_${_UPPACKAGE})
                                foreach(_sfx IN LISTS _suffixes)
                                    if(EXISTS ${_dir}/${_prefix}${_lib}${_sfx})
                                        list(APPEND _all_libs ${_dir}/${_prefix}${_lib}${_sfx})
                                        set(_this_found YES)
                                        if(NCBI_TRACE_COMPONENT_${DC_NAME} OR NCBI_TRACE_ALLCOMPONENTS)
                                            message("${DC_NAME}: found:  ${_dir}/${_prefix}${_lib}${_sfx}")
                                        endif()
                                        break()
                                    endif()
                                endforeach()
                                if(_this_found)
                                    break()
                                endif()
                            endforeach()
                            if(NOT _this_found)
                                list(APPEND _all_libs ${_lib})
                            endif()
                        endforeach()
                    endif()
                endif()
            else()
                message("ERROR: ${DC_NAME}: ${_package} not found")
            endif()
        endforeach()
        set(NCBI_COMPONENT_${DC_NAME}_LIBS ${_all_libs} PARENT_SCOPE)

        string(TOUPPER ${DC_NAME} _upname)
        set(HAVE_LIB${_upname} 1 PARENT_SCOPE)
        string(REPLACE "." "_" _altname ${_upname})
        set(HAVE_${_altname} 1 PARENT_SCOPE)

        list(APPEND NCBI_ALL_COMPONENTS ${DC_NAME})
        set(NCBI_ALL_COMPONENTS ${NCBI_ALL_COMPONENTS} PARENT_SCOPE)
        if(NCBI_TRACE_COMPONENT_${DC_NAME} OR NCBI_TRACE_ALLCOMPONENTS)
            message("----------------------")
            message("NCBI_define_Pkgcomponent: ${DC_NAME}")
            message("include: ${_include}")
            message("libs:    ${_all_libs}")
            message("defines: ${_defines}")
            message("----------------------")
        endif()
    else()
        if(NCBI_TRACE_COMPONENT_${DC_NAME} OR NCBI_TRACE_ALLCOMPONENTS)
            message("NOT FOUND ${DC_NAME}")
        endif()
    endif()
endfunction()

#############################################################################
#############################################################################

#############################################################################
# BACKWARD, UNWIND
NCBI_define_Pkgcomponent(NAME BACKWARD PACKAGE backward-cpp)
list(REMOVE_ITEM NCBI_ALL_COMPONENTS BACKWARD)
if(NCBI_COMPONENT_BACKWARD_FOUND)
    set(HAVE_LIBBACKWARD_CPP YES)
endif()
#NCBI_define_Pkgcomponent(NAME UNWIND PACKAGE libunwind)
#list(REMOVE_ITEM NCBI_ALL_COMPONENTS UNWIND)

#############################################################################
# LMDB
NCBI_define_Pkgcomponent(NAME LMDB PACKAGE lmdb)
if(NOT NCBI_COMPONENT_LMDB_FOUND)
    set(NCBI_COMPONENT_LMDB_FOUND ${NCBI_COMPONENT_LocalLMDB_FOUND})
    set(NCBI_COMPONENT_LMDB_INCLUDE ${NCBI_COMPONENT_LocalLMDB_INCLUDE})
    set(NCBI_COMPONENT_LMDB_NCBILIB ${NCBI_COMPONENT_LocalLMDB_NCBILIB})
    set(HAVE_LIBLMDB ${NCBI_COMPONENT_LMDB_FOUND})
endif()

#############################################################################
# PCRE
NCBI_define_Pkgcomponent(NAME PCRE PACKAGE pcre REQUIRES bzip2;zlib)
if(NOT NCBI_COMPONENT_PCRE_FOUND)
    set(NCBI_COMPONENT_PCRE_FOUND ${NCBI_COMPONENT_LocalPCRE_FOUND})
    set(NCBI_COMPONENT_PCRE_INCLUDE ${NCBI_COMPONENT_LocalPCRE_INCLUDE})
    set(NCBI_COMPONENT_PCRE_NCBILIB ${NCBI_COMPONENT_LocalPCRE_NCBILIB})
    set(HAVE_LIBPCRE ${NCBI_COMPONENT_PCRE_FOUND})
endif()

#############################################################################
# Z
NCBI_define_Pkgcomponent(NAME Z PACKAGE zlib)
if(NOT NCBI_COMPONENT_Z_FOUND)
    set(NCBI_COMPONENT_Z_FOUND ${NCBI_COMPONENT_LocalZ_FOUND})
    set(NCBI_COMPONENT_Z_INCLUDE ${NCBI_COMPONENT_LocalZ_INCLUDE})
    set(NCBI_COMPONENT_Z_NCBILIB ${NCBI_COMPONENT_LocalZ_NCBILIB})
    set(HAVE_LIBZ ${NCBI_COMPONENT_Z_FOUND})
endif()

#############################################################################
# BZ2
NCBI_define_Pkgcomponent(NAME BZ2 PACKAGE bzip2)
if(NOT NCBI_COMPONENT_BZ2_FOUND)
    set(NCBI_COMPONENT_BZ2_FOUND ${NCBI_COMPONENT_LocalBZ2_FOUND})
    set(NCBI_COMPONENT_BZ2_INCLUDE ${NCBI_COMPONENT_LocalBZ2_INCLUDE})
    set(NCBI_COMPONENT_BZ2_NCBILIB ${NCBI_COMPONENT_LocalBZ2_NCBILIB})
    set(HAVE_LIBBZ2 ${NCBI_COMPONENT_BZ2_FOUND})
endif()

#############################################################################
# LZO
NCBI_define_Pkgcomponent(NAME LZO PACKAGE lzo)

#############################################################################
# Boost
NCBI_define_Pkgcomponent(NAME Boost PACKAGE boost)
set(Boost_FOUND ${NCBI_COMPONENT_Boost_FOUND})

#############################################################################
# Boost.Test.Included
if(NCBI_COMPONENT_Boost_FOUND)
    set(NCBI_COMPONENT_Boost.Test.Included_FOUND YES)
    set(NCBI_COMPONENT_Boost.Test.Included_INCLUDE ${NCBI_COMPONENT_Boost_INCLUDE})
    set(NCBI_COMPONENT_Boost.Test.Included_DEFINES BOOST_TEST_NO_LIB)
    list(APPEND NCBI_ALL_COMPONENTS Boost.Test.Included)
else()
    set(NCBI_COMPONENT_Boost.Test.Included_FOUND NO)  
endif()

#############################################################################
# Boost.Test
if(NCBI_COMPONENT_Boost_FOUND)
    set(NCBI_COMPONENT_Boost.Test_FOUND YES)
    set(NCBI_COMPONENT_Boost.Test_INCLUDE ${NCBI_COMPONENT_Boost_INCLUDE})
    set(NCBI_COMPONENT_Boost.Test_LIBS    ${NCBI_COMPONENT_Boost_LIBS})
    set(NCBI_COMPONENT_Boost.Test_DEFINES ${NCBI_COMPONENT_Boost_DEFINES} BOOST_AUTO_LINK_NOMANGLE)
    list(APPEND NCBI_ALL_COMPONENTS Boost.Test)
else()
    set(NCBI_COMPONENT_Boost.Test_FOUND NO)
endif()

#############################################################################
# Boost.Spirit
if(NCBI_COMPONENT_Boost_FOUND)
    set(NCBI_COMPONENT_Boost.Spirit_FOUND YES)
    set(NCBI_COMPONENT_Boost.Spirit_INCLUDE ${NCBI_COMPONENT_Boost_INCLUDE})
    set(NCBI_COMPONENT_Boost.Spirit_LIBS    ${NCBI_COMPONENT_Boost_LIBS})
    set(NCBI_COMPONENT_Boost.Spirit_DEFINES ${NCBI_COMPONENT_Boost_DEFINES} BOOST_AUTO_LINK_NOMANGLE)
    list(APPEND NCBI_ALL_COMPONENTS Boost.Spirit)
else()
    set(NCBI_COMPONENT_Boost.Spirit_FOUND NO)
endif()

#############################################################################
# Boost.Thread
if(NCBI_COMPONENT_Boost_FOUND)
    set(NCBI_COMPONENT_Boost.Thread_FOUND YES)    
    set(NCBI_COMPONENT_Boost.Thread_INCLUDE ${NCBI_COMPONENT_Boost_INCLUDE})
    set(NCBI_COMPONENT_Boost.Thread_LIBS    ${Boost_LIBRARIES})
    set(NCBI_COMPONENT_Boost.Thread_DEFINES ${NCBI_COMPONENT_Boost_DEFINES} BOOST_AUTO_LINK_NOMANGLE)
    list(APPEND NCBI_ALL_COMPONENTS Boost.Thread)
else()
    set(NCBI_COMPONENT_Boost.Thread_FOUND NO)
endif()

#############################################################################
# JPEG
NCBI_define_Pkgcomponent(NAME JPEG PACKAGE libjpeg)

#############################################################################
# PNG
NCBI_define_Pkgcomponent(NAME PNG PACKAGE libpng REQUIRES zlib)

#############################################################################
# GIF
NCBI_define_Pkgcomponent(NAME GIF PACKAGE giflib)

#############################################################################
# TIFF
NCBI_define_Pkgcomponent(NAME TIFF PACKAGE libtiff REQUIRES zlib;libdeflate;xz_utils;libjpeg;jbig;zstd;libwebp)

#############################################################################
# SQLITE3
NCBI_define_Pkgcomponent(NAME SQLITE3 PACKAGE sqlite3)
if(NCBI_COMPONENT_SQLITE3_FOUND)
    check_symbol_exists(sqlite3_unlock_notify ${NCBI_COMPONENT_SQLITE3_INCLUDE}/sqlite3.h HAVE_SQLITE3_UNLOCK_NOTIFY)
    check_include_file(sqlite3async.h HAVE_SQLITE3ASYNC_H -I${NCBI_COMPONENT_SQLITE3_INCLUDE})
endif()

#############################################################################
# BerkeleyDB
NCBI_define_Pkgcomponent(NAME BerkeleyDB PACKAGE libdb)
if(NCBI_COMPONENT_BerkeleyDB_FOUND)
    set(HAVE_BERKELEY_DB 1)
    set(HAVE_BDB         1)
    set(HAVE_BDB_CACHE   1)
endif()

#############################################################################
# XML
NCBI_define_Pkgcomponent(NAME XML PACKAGE libxml2 REQUIRES zlib;libiconv)

#############################################################################
# XSLT
NCBI_define_Pkgcomponent(NAME XSLT PACKAGE libxslt REQUIRES libxml2;zlib;libiconv)
if(NOT DEFINED NCBI_XSLTPROCTOOL AND DEFINED CONAN_BIN_DIRS_LIBXSLT)
    set(NCBI_XSLTPROCTOOL "${CONAN_BIN_DIRS_LIBXSLT}/xsltproc${CMAKE_EXECUTABLE_SUFFIX}")
    set(NCBI_REQUIRE_XSLTPROCTOOL_FOUND YES)
endif()

#############################################################################
# EXSLT
NCBI_define_Pkgcomponent(NAME EXSLT PACKAGE libxslt REQUIRES libxml2;zlib;libiconv)

#############################################################################
# UV
NCBI_define_Pkgcomponent(NAME UV PACKAGE libuv)

#############################################################################
# NGHTTP2
NCBI_define_Pkgcomponent(NAME NGHTTP2 PACKAGE libnghttp2 REQUIRES zlib)

##############################################################################
# GRPC/PROTOBUF
if(NOT DEFINED NCBI_PROTOC_APP AND DEFINED CONAN_BIN_DIRS_PROTOBUF)
    set(NCBI_PROTOC_APP "${CONAN_BIN_DIRS_PROTOBUF}/protoc${CMAKE_EXECUTABLE_SUFFIX}")
endif()
if(NOT DEFINED NCBI_GRPC_PLUGIN AND DEFINED CONAN_BIN_DIRS_GRPC)
    set(NCBI_GRPC_PLUGIN "${CONAN_BIN_DIRS_GRPC}/grpc_cpp_plugin${CMAKE_EXECUTABLE_SUFFIX}")
endif()
NCBI_define_Pkgcomponent(NAME PROTOBUF PACKAGE protobuf REQUIRES zlib)
NCBI_define_Pkgcomponent(NAME GRPC PACKAGE grpc REQUIRES abseil;c-ares;openssl;protobuf;re2;zlib)

#############################################################################
# CASSANDRA
NCBI_define_Pkgcomponent(NAME CASSANDRA PACKAGE cassandra-cpp-driver REQUIRES http_parser;libuv;minizip;openssl;rapidjson;zlib)

#############################################################################
# MySQL
NCBI_define_Pkgcomponent(NAME MySQL PACKAGE libmysqlclient REQUIRES lz4;openssl;zlib;zstd)
