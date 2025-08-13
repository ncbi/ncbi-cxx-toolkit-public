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

option(USE_LOCAL_BZLIB "Use a local copy of libbz2")
option(USE_LOCAL_PCRE "Use a local copy of libpcre")
if(USE_LOCAL_BZLIB)
    set(NCBI_COMPONENT_BZ2_DISABLED TRUE)
endif()
if(USE_LOCAL_PCRE)
    set(NCBI_COMPONENT_PCRE_DISABLED TRUE)
    set(NCBI_COMPONENT_PCRE2_DISABLED TRUE)
endif()

#to debug
#set(NCBI_TRACE_COMPONENT_GRPC ON)

#############################################################################
# common settings
set(NCBI_OPT_ROOT  /opt/ncbi/64)

#############################################################################
# prebuilt libraries

set(NCBI_ThirdParty_BACKWARD      ${NCBI_TOOLS_ROOT}/backward-cpp-1.6-ncbi1 CACHE PATH "BACKWARD root")
set(NCBI_ThirdParty_UNWIND        ${NCBI_TOOLS_ROOT}/libunwind-1.1 CACHE PATH "UNWIND root")
set(NCBI_ThirdParty_LMDB          ${NCBI_TOOLS_ROOT}/lmdb-0.9.24 CACHE PATH "LMDB root")
set(NCBI_ThirdParty_LZO           ${NCBI_TOOLS_ROOT}/lzo-2.05 CACHE PATH "LZO root")
set(NCBI_ThirdParty_ZSTD          ${NCBI_TOOLS_ROOT}/zstd-1.5.2 CACHE PATH "ZSTD root")
set(NCBI_ThirdParty_Boost_VERSION "1.76.0")
set(NCBI_ThirdParty_Boost         ${NCBI_TOOLS_ROOT}/boost-${NCBI_ThirdParty_Boost_VERSION}-ncbi1 CACHE PATH "Boost root")
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(NCBI_ThirdParty_FASTCGI       ${NCBI_TOOLS_ROOT}/fcgi-2.4.2 CACHE PATH "FASTCGI root")
else()
  set(NCBI_ThirdParty_FASTCGI       ${NCBI_TOOLS_ROOT}/fcgi-2.4.0 CACHE PATH "FASTCGI root")
endif()
set(NCBI_ThirdParty_FASTCGI_SHLIB ${NCBI_ThirdParty_FASTCGI})
set(NCBI_ThirdParty_FASTCGIPP     ${NCBI_TOOLS_ROOT}/fastcgi++-3.1~a1+20210601 CACHE PATH "FASTCGIPP root")
set(NCBI_ThirdParty_SQLITE3       ${NCBI_TOOLS_ROOT}/sqlite-3.26.0-ncbi1 CACHE PATH "SQLITE3 root")
set(NCBI_ThirdParty_BerkeleyDB    ${NCBI_TOOLS_ROOT}/BerkeleyDB-5.3.28-ncbi1 CACHE PATH "BerkeleyDB root")
set(NCBI_ThirdParty_PYTHON_VERSION "3.11")
set(NCBI_ThirdParty_PYTHON        "/opt/python-${NCBI_ThirdParty_PYTHON_VERSION}" CACHE PATH "PYTHON root")
set(NCBI_ThirdParty_VDB           ${NCBI_OPT_ROOT}/trace_software/vdb/vdb-versions/cxx_toolkit/3 CACHE PATH "VDB root")
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(NCBI_ThirdParty_XML           ${NCBI_TOOLS_ROOT}/libxml-2.9.1 CACHE PATH "XML root")
  set(NCBI_ThirdParty_XSLT          ${NCBI_TOOLS_ROOT}/libxml-2.9.1 CACHE PATH "XSLT root")
else()
  set(NCBI_ThirdParty_XML           ${NCBI_TOOLS_ROOT}/libxml-2.7.8 CACHE PATH "XML root")
  set(NCBI_ThirdParty_XSLT          ${NCBI_TOOLS_ROOT}/libxml-2.7.8 CACHE PATH "XSLT root")
endif()
set(NCBI_ThirdParty_EXSLT         ${NCBI_ThirdParty_XSLT})
set(NCBI_ThirdParty_XLSXWRITER    ${NCBI_TOOLS_ROOT}/libxlsxwriter-0.6.9 CACHE PATH "XLSXWRITER root")
set(NCBI_ThirdParty_SAMTOOLS      ${NCBI_TOOLS_ROOT}/samtools CACHE PATH "SAMTOOLS root")
set(NCBI_ThirdParty_FTGL          ${NCBI_TOOLS_ROOT}/ftgl-2.1.3-rc5 CACHE PATH "FTGL root")
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(NCBI_ThirdParty_OpenGL        ${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2 CACHE PATH "OpenGL root")
  set(NCBI_ThirdParty_OSMesa        ${NCBI_ThirdParty_OpenGL})
  set(NCBI_ThirdParty_GLEW          ${NCBI_TOOLS_ROOT}/glew-2.2.0-ncbi1 CACHE PATH "GLEW root")
else()
  set(NCBI_ThirdParty_GLEW          ${NCBI_TOOLS_ROOT}/glew-1.5.8 CACHE PATH "GLEW root")
endif()
set(NCBI_ThirdParty_XERCES        ${NCBI_TOOLS_ROOT}/xerces-3.1.2 CACHE PATH "XERCES root")
set(NCBI_ThirdParty_GRPC          ${NCBI_TOOLS_ROOT}/grpc-1.50.2-ncbi1 CACHE PATH "GRPC root")
set(NCBI_ThirdParty_Abseil        ${NCBI_ThirdParty_GRPC})
set(NCBI_ThirdParty_Boring        ${NCBI_ThirdParty_GRPC})
set(NCBI_ThirdParty_PROTOBUF      ${NCBI_TOOLS_ROOT}/grpc-1.50.2-ncbi1 CACHE PATH "PROTOBUF root")
set(NCBI_ThirdParty_GFlags        ${NCBI_TOOLS_ROOT}/grpc-1.28.1-ncbi1 CACHE PATH "GFlags root")
set(NCBI_ThirdParty_XALAN         ${NCBI_TOOLS_ROOT}/xalan-1.11 CACHE PATH "XALAN root")
set(NCBI_ThirdParty_GPG           ${NCBI_TOOLS_ROOT}/libgpg-error-1.6 CACHE PATH "GPG root")
set(NCBI_ThirdParty_GCRYPT        ${NCBI_TOOLS_ROOT}/libgcrypt-1.4.3 CACHE PATH "GCRYPT root")
set(NCBI_ThirdParty_MSGSL         ${NCBI_TOOLS_ROOT}/msgsl-0.0.20171114-1c95f94 CACHE PATH "MSGSL root")
set(NCBI_ThirdParty_SGE           "/netmnt/gridengine/current/drmaa" CACHE PATH "SGE root")
set(NCBI_ThirdParty_MONGOCXX      ${NCBI_TOOLS_ROOT}/mongodb-3.7.0 CACHE PATH "MONGOCXX root")
set(NCBI_ThirdParty_MONGOC        ${NCBI_TOOLS_ROOT}/mongo-c-driver-1.23.2 CACHE PATH "MONGOC root")
set(NCBI_ThirdParty_LEVELDB       ${NCBI_TOOLS_ROOT}/leveldb-1.21 CACHE PATH "LEVELDB root")
set(NCBI_ThirdParty_URing         ${NCBI_TOOLS_ROOT}/liburing-2.4 CACHE PATH "URing root")
set(NCBI_ThirdParty_ROCKSDB       ${NCBI_TOOLS_ROOT}/rocksdb-8.3.2 CACHE PATH "ROCKSDB root")
set(NCBI_ThirdParty_wxWidgets     ${NCBI_TOOLS_ROOT}/wxWidgets-3.2.1-ncbi1 CACHE PATH "wxWidgets root")
set(NCBI_ThirdParty_PNG           ${NCBI_ThirdParty_wxWidgets} CACHE PATH "PNG root")
set(NCBI_ThirdParty_TIFF          ${NCBI_ThirdParty_wxWidgets} CACHE PATH "TIFF root")
set(NCBI_ThirdParty_GLPK          "/usr/local/glpk/4.45" CACHE PATH "GLPK root")
if(APPLE)
  set(NCBI_ThirdParty_UV            ${NCBI_TOOLS_ROOT}/libuv-1.44.2 CACHE PATH "UV root")
else()
  set(NCBI_ThirdParty_UV            ${NCBI_TOOLS_ROOT}/libuv-1.42.0 CACHE PATH "UV root")
endif()
set(NCBI_ThirdParty_NGHTTP2       ${NCBI_TOOLS_ROOT}/nghttp2-1.40.0 CACHE PATH "NGHTTP2 root")
set(NCBI_ThirdParty_GL2PS         ${NCBI_TOOLS_ROOT}/gl2ps-1.4.0 CACHE PATH "GL2PS root")
set(NCBI_ThirdParty_GMOCK         ${NCBI_TOOLS_ROOT}/googletest-1.8.1 CACHE PATH "GMOCK root")
set(NCBI_ThirdParty_GTEST         ${NCBI_TOOLS_ROOT}/googletest-1.8.1 CACHE PATH "GTEST root")
set(NCBI_ThirdParty_CASSANDRA     ${NCBI_TOOLS_ROOT}/datastax-cpp-driver-2.15.1 CACHE PATH "CASSANDRA root")
set(NCBI_ThirdParty_H2O           ${NCBI_TOOLS_ROOT}/h2o-2.2.6-ncbi3 CACHE PATH "H2O root")
set(NCBI_ThirdParty_GMP           ${NCBI_TOOLS_ROOT}/gmp-6.3.0 CACHE PATH "GMP root")
set(NCBI_ThirdParty_NETTLE        ${NCBI_TOOLS_ROOT}/nettle-3.10.2 CACHE PATH "NETTLE root")
set(NCBI_ThirdParty_GNUTLS        ${NCBI_TOOLS_ROOT}/gnutls-3.8.10-ncbi1 CACHE PATH "GNUTLS root")
set(NCBI_ThirdParty_NCBICRYPT     ${NCBI_TOOLS_ROOT}/ncbicrypt-20230516 CACHE PATH "NCBICRYPT root")

set(NCBI_ThirdParty_THRIFT        ${NCBI_TOOLS_ROOT}/thrift-0.11.0 CACHE PATH "THRIFT root")
set(NCBI_ThirdParty_NLohmann_JSON ${NCBI_TOOLS_ROOT}/nlohmann-json-3.9.1 CACHE PATH "NLohmann_JSON root")
set(NCBI_ThirdParty_YAML_CPP      ${NCBI_TOOLS_ROOT}/yaml-cpp-0.6.3 CACHE PATH "YAML_CPP root")
set(NCBI_ThirdParty_OPENTRACING   ${NCBI_TOOLS_ROOT}/opentracing-cpp-1.6.0 CACHE PATH "OPENTRACING root")
set(NCBI_ThirdParty_JAEGER        ${NCBI_TOOLS_ROOT}/jaeger-client-cpp-0.7.0 CACHE PATH "JAEGER root")
set(NCBI_ThirdParty_OPENTELEMETRY ${NCBI_TOOLS_ROOT}/opentelemetry-cpp-1.14.2+grpc150 CACHE PATH "OPENTELEMETRY root")
set(NCBI_ThirdParty_AWS_SDK       ${NCBI_TOOLS_ROOT}/aws-sdk-cpp-1.8.14 CACHE PATH "AWS_SDK root")

#############################################################################
#############################################################################


#############################################################################
# in-house-resources et al.
if (NOT NCBI_COMPONENT_in-house-resources_DISABLED)
    if (EXISTS "${NCBI_TOOLS_ROOT}/.ncbirc")
        if (EXISTS "/am/ncbiapdata/test_data")
            set(NCBITEST_TESTDATA_PATH "/am/ncbiapdata/test_data")
            set(NCBI_REQUIRE_in-house-resources_FOUND YES)
        elseif (EXISTS "/Volumes/ncbiapdata/test_data")
            set(NCBITEST_TESTDATA_PATH "/Volumes/ncbiapdata/test_data")
            set(NCBI_REQUIRE_in-house-resources_FOUND YES)
        endif()
        if(EXISTS "${NCBITEST_TESTDATA_PATH}/traces04")
            set(NCBI_REQUIRE_full-test-data_FOUND YES)
        endif()
        file(STRINGS "${NCBI_TOOLS_ROOT}/.ncbirc" blastdb_line
             REGEX "^[Bb][Ll][Aa][Ss][Tt][Dd][Bb] *=")
        string(REGEX REPLACE "^[Bb][Ll][Aa][Ss][Tt][Dd][Bb] *= *" ""
               blastdb_path "${blastdb_line}")
        if(EXISTS "${blastdb_path}")
            set(NCBI_REQUIRE_full-blastdb_FOUND YES)
        endif()
    endif()
endif()
NCBIcomponent_report(in-house-resources)
NCBIcomponent_report(full-test-data)
NCBIcomponent_report(full-blastdb)

#############################################################################
# NCBI_C
if(NOT NCBI_COMPONENT_NCBI_C_DISABLED)
    set(NCBI_C_ROOT "${NCBI_TOOLS_ROOT}/ncbi")

    get_directory_property(_foo_defs COMPILE_DEFINITIONS)
    if("${_foo_defs}" MATCHES NCBI_INT4_GI)
        set(NCBI_CTOOLKIT_PATH "${NCBI_C_ROOT}")
        set(USE_BIGINT_IDS 0)
    else()
        if (EXISTS "${NCBI_C_ROOT}/ncbi.gi64")
            set(NCBI_CTOOLKIT_PATH "${NCBI_C_ROOT}/ncbi.gi64")
        elseif (EXISTS "${NCBI_C_ROOT}.gi64")
            set(NCBI_CTOOLKIT_PATH "${NCBI_C_ROOT}.gi64")
        else ()
            set(NCBI_CTOOLKIT_PATH "${NCBI_C_ROOT}")
        endif ()
        set(USE_BIGINT_IDS 1)
    endif()

    if(EXISTS "${NCBI_CTOOLKIT_PATH}/include64" AND EXISTS "${NCBI_CTOOLKIT_PATH}/lib64")
        set(NCBI_C_INCLUDE  "${NCBI_CTOOLKIT_PATH}/include64")
        set(_connectless TRUE)
# alt == Debug
        foreach( _type IN ITEMS "" "alt")
            set(NCBI_C_LIBPATH${_type}  "${NCBI_CTOOLKIT_PATH}/${_type}lib64")
            set(_lib${_type} "${NCBI_CTOOLKIT_PATH}/${_type}lib64/libncbi_connectless.a")
            if(NOT EXISTS "${_lib${_type}}")
                set(_lib${_type} "${NCBI_CTOOLKIT_PATH}/${_type}lib64/libncbi.a")
                set(_connectless FALSE)
            endif()
        endforeach()
        add_library(ncbi_corelib STATIC IMPORTED GLOBAL)
        set_property(TARGET ncbi_corelib PROPERTY IMPORTED_LOCATION "${_lib}")
        foreach(_cfg IN ITEMS ${NCBI_CONFIGURATION_TYPES} Debug Release)
            NCBI_util_Cfg_ToStd(${_cfg} _std)
            string(TOUPPER ${_cfg} _upcfg)
            if("${_std}" STREQUAL "Debug")
                set_property(TARGET ncbi_corelib PROPERTY IMPORTED_LOCATION_${_upcfg} ${_libalt})
            else()
                set_property(TARGET ncbi_corelib PROPERTY IMPORTED_LOCATION_${_upcfg} ${_lib})
            endif()
        endforeach()
        if(APPLE)
            set_property(TARGET ncbi_corelib PROPERTY INTERFACE_LINK_OPTIONS 
                -L$<IF:$<CONFIG:${NCBI_DEBUG_CONFIGURATION_TYPES_STRING}>,${NCBI_C_LIBPATHalt},${NCBI_C_LIBPATH}>
                -Wl,-framework,ApplicationServices)
        else()
            set_property(TARGET ncbi_corelib PROPERTY INTERFACE_LINK_OPTIONS 
                -L$<IF:$<CONFIG:${NCBI_DEBUG_CONFIGURATION_TYPES_STRING}>,${NCBI_C_LIBPATHalt},${NCBI_C_LIBPATH}>)
        endif()
        set(NCBI_C_ncbi ncbi_corelib)
        if(_connectless)
            set(NCBI_C_ncbi ncbi_corelib xconnect)
            set(NCBI_COMPONENT_NCBI_C_NCBILIB xconnect)
        endif()
        set(HAVE_NCBI_C YES)
    else()
        set(HAVE_NCBI_C NO)
    endif()

    if(HAVE_NCBI_C)
        NCBI_notice("NCBI_C found at ${NCBI_C_INCLUDE}")
        set(NCBI_COMPONENT_NCBI_C_FOUND YES)
        set(NCBI_COMPONENT_NCBI_C_INCLUDE ${NCBI_C_INCLUDE})
        set(_c_libs  ncbiobj ncbimmdb ${NCBI_C_ncbi})
        set(NCBI_COMPONENT_NCBI_C_LIBS ${_c_libs})
        set(NCBI_COMPONENT_NCBI_C_DEFINES HAVE_NCBI_C=1
            USE_BIGINT_IDS=${USE_BIGINT_IDS})
        set(NCBI_COMPONENT_NCBI_C_LIBPATH ${NCBI_C_LIBPATH})
    else()
        set(NCBI_COMPONENT_NCBI_C_FOUND NO)
        NCBI_notice("NOT FOUND NCBI_C")
    endif()
endif()
NCBIcomponent_report(NCBI_C)

#############################################################################
# BACKWARD, UNWIND
NCBI_define_Xcomponent(NAME BACKWARD)
NCBIcomponent_report(BACKWARD)
if(NCBI_COMPONENT_BACKWARD_FOUND)
    set(HAVE_LIBBACKWARD_CPP YES)
    if(NOT NCBI_PTBCFG_USECONAN AND NOT NCBI_PTBCFG_HASCONAN AND NOT NCBI_PTBCFG_PACKAGING AND NOT NCBI_PTBCFG_PACKAGED)
        NCBI_find_system_library(LIBDW_LIBS dw)
        if (LIBDW_LIBS)
            set(HAVE_LIBDW 1)
            set(NCBI_COMPONENT_BACKWARD_LIBS ${NCBI_COMPONENT_BACKWARD_LIBS} ${LIBDW_LIBS})
        endif()
    endif()
endif()
if(NOT HAVE_BACKWARD_HPP)
    check_include_file_cxx(backward.hpp HAVE_BACKWARD_HPP
        -I${NCBI_COMPONENT_BACKWARD_INCLUDE})
endif()
if(NOT NCBI_COMPONENT_UNWIND_FOUND)
    if(NOT CYGWIN OR (DEFINED NCBI_COMPONENT_UNWIND_DISABLED AND NOT NCBI_COMPONENT_UNWIND_DISABLED))
        check_include_file(libunwind.h HAVE_LIBUNWIND_H)
        if(HAVE_LIBUNWIND_H)
            NCBI_define_Xcomponent(NAME UNWIND MODULE libunwind LIB unwind)
        endif()
    endif()
endif()

############################################################################
# Kerberos 5 (via GSSAPI)
NCBI_define_Xcomponent(NAME KRB5 LIB gssapi_krb5 krb5 k5crypto com_err CHECK_INCLUDE gssapi/gssapi_krb5.h)
if(NCBI_COMPONENT_KRB5_FOUND)
    set(KRB5_INCLUDE ${NCBI_COMPONENT_KRB5_INCLUDE})
    set(KRB5_LIBS ${NCBI_COMPONENT_KRB5_LIBS})
endif()
NCBIcomponent_report(KRB5)

##############################################################################
# UUID
if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    set(_check uuid.h)
else()
    set(_check uuid/uuid.h)
endif()
NCBI_define_Xcomponent(NAME UUID MODULE uuid LIB uuid CHECK_INCLUDE ${_check})
NCBIcomponent_report(UUID)

##############################################################################
# CURL
NCBI_define_Xcomponent(NAME CURL MODULE libcurl PACKAGE CURL LIB curl CHECK_INCLUDE curl/curl.h)
if(NCBI_COMPONENT_CURL_FOUND AND NOT TARGET CURL::libcurl)
    add_library(CURL::libcurl UNKNOWN IMPORTED GLOBAL)
    set_target_properties(CURL::libcurl PROPERTIES
        IMPORTED_LOCATION ${NCBI_COMPONENT_CURL_LIBS}
        IMPORTED_LOCATION_DEBUG ${NCBI_COMPONENT_CURL_LIBS}
        IMPORTED_LOCATION_RELEASE ${NCBI_COMPONENT_CURL_LIBS}
    )
endif()
NCBIcomponent_report(CURL)

#############################################################################
# LMDB
NCBI_define_Xcomponent(NAME LMDB LIB lmdb)
NCBIcomponent_report(LMDB)
if(NOT NCBI_COMPONENT_LMDB_FOUND)
    set(NCBI_COMPONENT_LMDB_FOUND ${NCBI_COMPONENT_LocalLMDB_FOUND})
    set(NCBI_COMPONENT_LMDB_INCLUDE ${NCBI_COMPONENT_LocalLMDB_INCLUDE})
    set(NCBI_COMPONENT_LMDB_NCBILIB ${NCBI_COMPONENT_LocalLMDB_NCBILIB})
endif()
set(HAVE_LIBLMDB ${NCBI_COMPONENT_LMDB_FOUND})

#############################################################################
# PCRE
NCBI_define_Xcomponent(NAME PCRE MODULE libpcre LIB pcre CHECK_INCLUDE pcre.h)
NCBIcomponent_report(PCRE)

NCBI_define_Xcomponent(NAME PCRE2 MODULE libpcre2 LIB pcre2-8)
NCBIcomponent_report(PCRE2)

if(EXISTS ${NCBITK_INC_ROOT}/util/regexp/pcre.h
   AND NOT NCBI_COMPONENT_PCRE_FOUND
   AND NOT NCBI_COMPONENT_PCRE2_FOUND)
    set(NCBI_COMPONENT_PCRE_FOUND ${NCBI_COMPONENT_LocalPCRE_FOUND})
    set(NCBI_COMPONENT_PCRE_INCLUDE ${NCBI_COMPONENT_LocalPCRE_INCLUDE})
    set(NCBI_COMPONENT_PCRE_NCBILIB ${NCBI_COMPONENT_LocalPCRE_NCBILIB})
endif()
set(HAVE_LIBPCRE ${NCBI_COMPONENT_PCRE_FOUND})

if(EXISTS ${NCBITK_INC_ROOT}/util/regexp/pcre2.h
   AND NOT NCBI_COMPONENT_PCRE_FOUND
   AND NOT NCBI_COMPONENT_PCRE2_FOUND)
    set(NCBI_COMPONENT_PCRE2_FOUND ${NCBI_COMPONENT_LocalPCRE_FOUND})
    set(NCBI_COMPONENT_PCRE2_INCLUDE ${NCBI_COMPONENT_LocalPCRE_INCLUDE})
    set(NCBI_COMPONENT_PCRE2_NCBILIB ${NCBI_COMPONENT_LocalPCRE_NCBILIB})
endif()
set(HAVE_LIBPCRE2 ${NCBI_COMPONENT_PCRE2_FOUND})

#############################################################################
# Z
NCBI_define_Xcomponent(NAME Z MODULE zlib PACKAGE ZLIB LIB z CHECK_INCLUDE zlib.h)
NCBIcomponent_report(Z)
if(NOT NCBI_COMPONENT_Z_FOUND)
    set(NCBI_COMPONENT_Z_FOUND ${NCBI_COMPONENT_LocalZ_FOUND})
    set(NCBI_COMPONENT_Z_INCLUDE ${NCBI_COMPONENT_LocalZ_INCLUDE})
    set(NCBI_COMPONENT_Z_NCBILIB ${NCBI_COMPONENT_LocalZ_NCBILIB})
endif()
set(HAVE_LIBZ ${NCBI_COMPONENT_Z_FOUND})

#############################################################################
# BZ2
NCBI_define_Xcomponent(NAME BZ2 PACKAGE BZip2 LIB bz2 CHECK_INCLUDE bzlib.h)
NCBIcomponent_report(BZ2)
if(NOT NCBI_COMPONENT_BZ2_FOUND)
    set(NCBI_COMPONENT_BZ2_FOUND ${NCBI_COMPONENT_LocalBZ2_FOUND})
    set(NCBI_COMPONENT_BZ2_INCLUDE ${NCBI_COMPONENT_LocalBZ2_INCLUDE})
    set(NCBI_COMPONENT_BZ2_NCBILIB ${NCBI_COMPONENT_LocalBZ2_NCBILIB})
endif()
set(HAVE_LIBBZ2 ${NCBI_COMPONENT_BZ2_FOUND})

#############################################################################
# LZO
NCBI_define_Xcomponent(NAME LZO LIB lzo2 CHECK_INCLUDE lzo/lzo1x.h)
NCBIcomponent_report(LZO)

#############################################################################
# ZSTD
NCBI_define_Xcomponent(NAME ZSTD LIB zstd CHECK_INCLUDE zstd.h)
NCBIcomponent_report(ZSTD)
if(NCBI_COMPONENT_ZSTD_FOUND AND
    (DEFINED NCBI_COMPONENT_ZSTD_VERSION AND "${NCBI_COMPONENT_ZSTD_VERSION}" VERSION_LESS "1.4"))
    message("ZSTD: Version requirement not met (required at least v1.4)")
    set(NCBI_COMPONENT_ZSTD_FOUND NO)
    set(HAVE_LIBZSTD 0)
endif()

#############################################################################
# BOOST
if(NOT NCBI_COMPONENT_Boost_DISABLED AND NOT NCBI_COMPONENT_Boost_FOUND)
    include(${NCBI_TREE_CMAKECFG}/CMakeChecks.boost.cmake)
endif()
NCBIcomponent_report(Boost.Test.Included)
NCBIcomponent_report(Boost.Test)
NCBIcomponent_report(Boost.Spirit)
NCBIcomponent_report(Boost.Thread)
NCBIcomponent_report(Boost)

#############################################################################
# JPEG
NCBI_define_Xcomponent(NAME JPEG MODULE libjpeg PACKAGE JPEG LIB jpeg CHECK_INCLUDE jpeglib.h)
NCBIcomponent_report(JPEG)

#############################################################################
# PNG
NCBI_define_Xcomponent(NAME PNG MODULE libpng PACKAGE PNG LIB png CHECK_INCLUDE png.h)
NCBIcomponent_report(PNG)

#############################################################################
# GIF (temporarily disabled)
# NCBI_define_Xcomponent(NAME GIF PACKAGE GIF LIB gif)
# NCBIcomponent_report(GIF)

#############################################################################
# TIFF
NCBI_define_Xcomponent(NAME TIFF MODULE libtiff-4 PACKAGE TIFF LIB tiff CHECK_INCLUDE tiffio.h)
NCBIcomponent_report(TIFF)

#############################################################################
# FASTCGI
NCBI_define_Xcomponent(NAME FASTCGI LIB fcgi)
NCBIcomponent_report(FASTCGI)

#############################################################################
# FASTCGIPP
NCBI_define_Xcomponent(NAME FASTCGIPP LIB fastcgipp)
NCBIcomponent_report(FASTCGIPP)

#############################################################################
# SQLITE3
if(NOT NCBI_COMPONENT_SQLITE3_FOUND)
    NCBI_define_Xcomponent(NAME SQLITE3 MODULE sqlite3 PACKAGE SQLite3 LIB sqlite3)
    if(NCBI_COMPONENT_SQLITE3_FOUND)
        check_symbol_exists(sqlite3_unlock_notify ${NCBI_COMPONENT_SQLITE3_INCLUDE}/sqlite3.h HAVE_SQLITE3_UNLOCK_NOTIFY)
        check_include_file(sqlite3async.h HAVE_SQLITE3ASYNC_H -I${NCBI_COMPONENT_SQLITE3_INCLUDE})
    endif()
endif()
NCBIcomponent_report(SQLITE3)

#############################################################################
# BerkeleyDB
NCBI_define_Xcomponent(NAME BerkeleyDB LIB db CHECK_INCLUDE db.h)
if(NCBI_COMPONENT_BerkeleyDB_FOUND)
    set(HAVE_BERKELEY_DB 1)
    set(HAVE_BDB         1)
    set(HAVE_BDB_CACHE   1)
endif()
NCBIcomponent_report(BerkeleyDB)

#############################################################################
# ODBC
set(NCBI_COMPONENT_ODBC_FOUND NO)
if(EXISTS  ${NCBITK_INC_ROOT}/dbapi/driver/odbc/unix_odbc)
    set(NCBI_COMPONENT_XODBC_FOUND YES)
    set(NCBI_COMPONENT_XODBC_INCLUDE ${NCBITK_INC_ROOT}/dbapi/driver/odbc/unix_odbc)
endif()

#############################################################################
# MySQL
#NCBI_define_Xcomponent(NAME MySQL PACKAGE Mysql LIB mysqlclient LIBPATH_SUFFIX mysql INCLUDE mysql/mysql.h)
NCBI_define_Xcomponent(NAME MySQL LIB mysqlclient LIBPATH_SUFFIX mysql INCLUDE mysql/mysql.h)
NCBIcomponent_report(MySQL)

#############################################################################
# PYTHON
NCBI_define_Xcomponent(NAME PYTHON LIB python${NCBI_ThirdParty_PYTHON_VERSION} python3 INCLUDE python${NCBI_ThirdParty_PYTHON_VERSION})
NCBIcomponent_report(PYTHON)

#############################################################################
# VDB
if(NOT NCBI_COMPONENT_VDB_DISABLED AND NOT NCBI_COMPONENT_VDB_FOUND)
    if("${HOST_OS}" MATCHES "linux")
        set(NCBI_ThirdParty_VDB_OS linux)
    elseif("${HOST_OS}" MATCHES "darwin")
        set(NCBI_ThirdParty_VDB_OS mac)
    endif()
    if("${HOST_CPU}" MATCHES "x86")
        set(NCBI_ThirdParty_VDB_ARCH x86_64)
    elseif("${HOST_CPU}" MATCHES "arm" OR "${HOST_CPU}" MATCHES "aarch64")
        set(NCBI_ThirdParty_VDB_ARCH arm64)
    endif()
    if("${NCBI_COMPILER}" MATCHES "CLANG")
        set(NCBI_ThirdParty_VDB_COMPILER clang)
    else() # Specifically check for "GCC"?
        set(NCBI_ThirdParty_VDB_COMPILER gcc)
    endif()
    NCBI_define_Xcomponent(NAME VDB LIB ncbi-vdb
        LIBPATH_SUFFIX ${NCBI_ThirdParty_VDB_OS}/release/${NCBI_ThirdParty_VDB_ARCH}/lib
#        LIBPATH_SUFFIX ${NCBI_ThirdParty_VDB_OS}/$<LOSTDCONFIG>/${NCBI_ThirdParty_VDB_ARCH}/lib
        INCPATH_SUFFIX interfaces)
    if(NCBI_COMPONENT_VDB_FOUND)
        set(NCBI_COMPONENT_VDB_INCLUDE
            ${NCBI_COMPONENT_VDB_INCLUDE} 
            ${NCBI_COMPONENT_VDB_INCLUDE}/os/${NCBI_ThirdParty_VDB_OS}
            ${NCBI_COMPONENT_VDB_INCLUDE}/os/unix
            ${NCBI_COMPONENT_VDB_INCLUDE}/cc/${NCBI_ThirdParty_VDB_COMPILER}/${NCBI_ThirdParty_VDB_ARCH}
            ${NCBI_COMPONENT_VDB_INCLUDE}/cc/${NCBI_ThirdParty_VDB_COMPILER}
        )
        set(HAVE_NCBI_VDB 1)
    endif()
endif()
NCBIcomponent_report(VDB)

##############################################################################
# wxWidgets
if(NOT NCBI_COMPONENT_wxWidgets_DISABLED AND NOT NCBI_COMPONENT_wxWidgets_FOUND)
    include(${NCBI_TREE_CMAKECFG}/CMakeChecks.wxWidgets.cmake)
endif()

if(NOT NCBI_COMPONENT_wxWidgets_FOUND)
    NCBI_define_Xcomponent(NAME GTK2 PACKAGE GTK2)
    NCBI_define_Xcomponent(NAME FONTCONFIG MODULE fontconfig PACKAGE Fontconfig LIB fontconfig)
    set(_wx_ver 3.2)
    NCBI_define_Xcomponent(NAME wxWidgets LIB
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
        INCLUDE wx-${_wx_ver} ADD_COMPONENT FONTCONFIG GTK2
    )
    if(NCBI_COMPONENT_wxWidgets_FOUND)
        list(GET NCBI_COMPONENT_wxWidgets_LIBS 0 _lib)
        get_filename_component(_libdir ${_lib} DIRECTORY)
        set(NCBI_COMPONENT_wxWidgets_INCLUDE ${_libdir}/wx/include/gtk2-ansi-${_wx_ver} ${NCBI_COMPONENT_wxWidgets_INCLUDE})
        set(NCBI_COMPONENT_wxWidgets_LIBS    ${NCBI_COMPONENT_wxWidgets_LIBS} -lXxf86vm -lSM -lexpat)
        if(BUILD_SHARED_LIBS)
            set(NCBI_COMPONENT_wxWidgets_DEFINES __WXGTK__  WXUSINGDLL wxDEBUG_LEVEL=0)
        else()
            set(NCBI_COMPONENT_wxWidgets_DEFINES __WXGTK__ wxDEBUG_LEVEL=0)
        endif()
    else()
        NCBI_define_Xcomponent(NAME wxWidgets PACKAGE wxWidgets)
    endif()
endif()
NCBIcomponent_report(wxWidgets)

##############################################################################
# GCRYPT
NCBI_define_Xcomponent(NAME GPG    LIB gpg-error CHECK_INCLUDE gpg-error.h)
NCBI_define_Xcomponent(NAME GCRYPT LIB gcrypt ADD_COMPONENT GPG CHECK_INCLUDE gcrypt.h)
NCBIcomponent_report(GCRYPT)

#############################################################################
# XML
NCBI_define_Xcomponent(NAME XML MODULE libxml-2.0 PACKAGE LibXml2 LIB xml2 INCLUDE libxml2 CHECK_INCLUDE libxml/xmlexports.h)
NCBIcomponent_report(XML)

#############################################################################
# XSLT
NCBI_define_Xcomponent(NAME XSLT MODULE libxslt PACKAGE LibXslt LIB xslt ADD_COMPONENT XML CHECK_INCLUDE libxslt/xslt.h)
NCBIcomponent_report(XSLT)
if(NCBI_COMPONENT_XSLT_FOUND)
    if(NOT NCBI_XSLTPROCTOOL)
        if(LIBXSLT_XSLTPROC_EXECUTABLE)
            set(NCBI_XSLTPROCTOOL ${LIBXSLT_XSLTPROC_EXECUTABLE})
        elseif(EXISTS ${NCBI_ThirdParty_XSLT}/bin/xsltproc)
            set(NCBI_XSLTPROCTOOL ${NCBI_ThirdParty_XSLT}/bin/xsltproc)
        else()
            find_program(NCBI_XSLTPROCTOOL xsltproc)
        endif()
        if(NCBI_XSLTPROCTOOL)
            set(NCBI_REQUIRE_XSLTPROCTOOL_FOUND YES)
        endif()
    endif()
endif()
if(NCBI_TRACE_COMPONENT_XSLT OR NCBI_TRACE_ALLCOMPONENTS)
    message("NCBI_XSLTPROCTOOL = ${NCBI_XSLTPROCTOOL}")
endif()

#############################################################################
# EXSLT
if(NOT NCBI_COMPONENT_EXSLT_FOUND)
    NCBI_define_Xcomponent(NAME EXSLT MODULE libexslt PACKAGE LibXslt LIB exslt ADD_COMPONENT XML GCRYPT CHECK_INCLUDE libexslt/exslt.h)
    if(NCBI_COMPONENT_EXSLT_FOUND)
        set(NCBI_COMPONENT_EXSLT_LIBS ${LIBXSLT_EXSLT_LIBRARIES} ${NCBI_COMPONENT_EXSLT_LIBS})
    endif()
endif()
NCBIcomponent_report(EXSLT)

#############################################################################
# XLSXWRITER
NCBI_define_Xcomponent(NAME XLSXWRITER LIB xlsxwriter)
NCBIcomponent_report(XLSXWRITER)

#############################################################################
# LAPACK
NCBI_define_Xcomponent(NAME LAPACK PACKAGE LAPACK LIB lapack blas)
if(NCBI_COMPONENT_LAPACK_FOUND)
    check_include_file(lapacke.h HAVE_LAPACKE_H)
    check_include_file(lapacke/lapacke.h HAVE_LAPACKE_LAPACKE_H)
    check_include_file(Accelerate/Accelerate.h HAVE_ACCELERATE_ACCELERATE_H)
endif()
NCBIcomponent_report(LAPACK)

#############################################################################
# SAMTOOLS
NCBI_define_Xcomponent(NAME SAMTOOLS LIB bam CHECK_INCLUDE bam.h)
NCBIcomponent_report(SAMTOOLS)

#############################################################################
# FreeType
NCBI_define_Xcomponent(NAME FreeType MODULE freetype2 PACKAGE Freetype LIB freetype INCLUDE freetype2)
NCBIcomponent_report(FreeType)

#############################################################################
# FTGL
NCBI_define_Xcomponent(NAME FTGL MODULE ftgl LIB ftgl INCLUDE FTGL)
NCBIcomponent_report(FTGL)

#############################################################################
# GLEW
#NCBI_define_Xcomponent(NAME GLEW MODULE glew LIB GLEW)
NCBI_define_Xcomponent(NAME GLEW LIB GLEW)
if(NCBI_COMPONENT_GLEW_FOUND)
    foreach( _inc IN LISTS NCBI_COMPONENT_GLEW_INCLUDE)
        get_filename_component(_incdir ${_inc} DIRECTORY)
        get_filename_component(_incGL ${_inc} NAME)
        if("${_incGL}" STREQUAL "GL")
            set(NCBI_COMPONENT_GLEW_INCLUDE ${_incdir} ${NCBI_COMPONENT_GLEW_INCLUDE})
            break()
        endif()
    endforeach()
    set(saved_REQUIRED_DEFINITIONS  ${CMAKE_REQUIRED_DEFINITIONS})
    set(saved_REQUIRED_INCLUDES     ${CMAKE_REQUIRED_INCLUDES})
    set(saved_REQUIRED_LINK_OPTIONS ${CMAKE_REQUIRED_LINK_OPTIONS})
    set(CMAKE_REQUIRED_DEFINITIONS  "-DGLEW_MX")
    set(CMAKE_REQUIRED_INCLUDES     ${NCBI_COMPONENT_GLEW_INCLUDE})
    set(CMAKE_REQUIRED_LINK_OPTIONS ${NCBI_COMPONENT_GLEW_LDFLAGS})
    check_symbol_exists(glewContextInit "GL/glew.h" HAVE_GLEW_MX)
    set(CMAKE_REQUIRED_DEFINITIONS  ${saved_REQUIRED_DEFINITIONS})
    set(CMAKE_REQUIRED_INCLUDES     ${saved_REQUIRED_INCLUDES})
    set(CMAKE_REQUIRED_LINK_OPTIONS ${saved_REQUIRED_LINK_OPTIONS})
    if(HAVE_GLEW_MX)
        set(NCBI_COMPONENT_GLEW_DEFINES ${NCBI_COMPONENT_GLEW_DEFINES} GLEW_MX)
    endif()
    if(NOT BUILD_SHARED_LIBS)
        set(NCBI_COMPONENT_GLEW_DEFINES ${NCBI_COMPONENT_GLEW_DEFINES} GLEW_STATIC)
    endif()
endif()
NCBIcomponent_report(GLEW)

##############################################################################
# OpenGL
set(OpenGL_GL_PREFERENCE LEGACY)
NCBI_define_Xcomponent(NAME OpenGL PACKAGE OpenGL LIB GLU GL)
if(NCBI_COMPONENT_OpenGL_FOUND)
    set(NCBI_COMPONENT_OpenGL_LIBS ${NCBI_COMPONENT_OpenGL_LIBS}  -lXmu -lXt -lXext -lX11)
endif()
NCBIcomponent_report(OpenGL)

##############################################################################
# OSMesa
NCBI_define_Xcomponent(NAME OSMesa LIB OSMesa ADD_COMPONENT OpenGL)
NCBIcomponent_report(OSMesa)

#############################################################################
# XERCES
NCBI_define_Xcomponent(NAME XERCES MODULE xerces-c PACKAGE XercesC LIB xerces-c)
NCBIcomponent_report(XERCES)

##############################################################################
# GRPC/PROTOBUF

NCBI_define_Xcomponent(NAME PROTOBUF CMAKE_PACKAGE Protobuf CMAKE_LIB libprotobuf
    PACKAGE Protobuf LIB protobuf CHECK_INCLUDE google/protobuf/stubs/platform_macros.h)
NCBIcomponent_report(PROTOBUF)
if(NOT NCBI_PROTOC_APP)
    if(TARGET protobuf::protoc)
        foreach( _type IN ITEMS "" "_RELEASE" "_DEBUG")
            get_property(NCBI_PROTOC_APP TARGET protobuf::protoc PROPERTY IMPORTED_LOCATION${_type})
            if(EXISTS "${NCBI_PROTOC_APP}")
                break()
            endif()
        endforeach()
    elseif(EXISTS "${NCBI_ThirdParty_GRPC}/bin/protoc")
        set(NCBI_PROTOC_APP "${NCBI_ThirdParty_GRPC}/bin/protoc")
    else()
        find_program(NCBI_PROTOC_APP protoc)
    endif()
endif()
if(NCBI_TRACE_COMPONENT_PROTOBUF OR NCBI_TRACE_ALLCOMPONENTS)
    message("NCBI_PROTOC_APP = ${NCBI_PROTOC_APP}")
endif()

NCBI_define_Xcomponent(NAME GRPC CMAKE_PACKAGE gRPC CMAKE_LIB grpc++)
if(NOT NCBI_COMPONENT_GRPC_FOUND)
    NCBI_define_Xcomponent(NAME Boring LIB boringssl boringcrypto)
    NCBI_define_Xcomponent(NAME GRPC MODULE grpc++ LIB
        grpc++ grpc address_sorting re2 upb cares
        absl_raw_hash_set absl_hashtablez_sampler absl_exponential_biased absl_hash
        absl_city absl_statusor absl_bad_variant_access gpr
        absl_status absl_cord absl_str_format_internal absl_synchronization absl_graphcycles_internal
        absl_symbolize absl_demangle_internal absl_stacktrace absl_debugging_internal absl_malloc_internal
        absl_time absl_time_zone absl_civil_time absl_strings absl_strings_internal absl_throw_delegate
        absl_int128 absl_base absl_spinlock_wait absl_bad_optional_access absl_raw_logging_internal absl_log_severity
        )
    if(NCBI_COMPONENT_GRPC_FOUND)
        set(NCBI_COMPONENT_GRPC_LIBS  ${NCBI_COMPONENT_GRPC_LIBS} ${NCBI_COMPONENT_Boring_LIBS})
        if(NOT NCBI_PTBCFG_USECONAN AND NOT NCBI_PTBCFG_HASCONAN AND NOT NCBI_PTBCFG_PACKAGING AND NOT NCBI_PTBCFG_PACKAGED AND
            "${NCBI_COMPONENT_GRPC_VERSION}" STRLESS "1.17.0")
            NCBI_define_Xcomponent(NAME CGRPC MODULE grpc LIB grpc)
            set(NCBI_COMPONENT_GRPC_LIBS ${NCBI_COMPONENT_GRPC_LIBS} ${NCBI_COMPONENT_CGRPC_LIBS})
        endif()
    endif()
endif()
NCBIcomponent_report(GRPC)
if(NOT NCBI_GRPC_PLUGIN)
    if(TARGET gRPC::grpc_cpp_plugin)
        foreach( _type IN ITEMS "" "_RELEASE" "_DEBUG")
            get_property(NCBI_GRPC_PLUGIN TARGET gRPC::grpc_cpp_plugin PROPERTY IMPORTED_LOCATION${_type})
            if(EXISTS "${NCBI_GRPC_PLUGIN}")
                break()
            endif()
        endforeach()
    elseif(EXISTS "${NCBI_ThirdParty_GRPC}/bin/grpc_cpp_plugin")
        set(NCBI_GRPC_PLUGIN "${NCBI_ThirdParty_GRPC}/bin/grpc_cpp_plugin")
    else()
        find_program(NCBI_GRPC_PLUGIN grpc_cpp_plugin)
    endif()
endif()
if(NCBI_TRACE_COMPONENT_GRPC OR NCBI_TRACE_ALLCOMPONENTS)
    message("NCBI_GRPC_PLUGIN = ${NCBI_GRPC_PLUGIN}")
endif()

# Explicitly cover a subset of Abseil, for the potential sake of OpenTelemetry
NCBI_define_Xcomponent(NAME Abseil CMAKE_PACKAGE absl
  CMAKE_LIB strings bad_variant_access any base bits city)

# Explicitly cover GFlags (from an older installation), for the
# potential sake of RocksDB
NCBI_define_Xcomponent(NAME GFlags CMAKE_PACKAGE gflags CMAKE_LIB gflags)

#############################################################################
# XALAN
NCBI_define_Xcomponent(NAME XALAN PACKAGE XalanC LIB xalan-c xalanMsg)
NCBIcomponent_report(XALAN)

##############################################################################
# PERL
if(NOT NCBI_COMPONENT_PERL_DISABLED AND NOT NCBI_COMPONENT_PERL_FOUND)
    find_package(PerlLibs)
    if (PERLLIBS_FOUND)
        set(NCBI_COMPONENT_PERL_FOUND   YES)
        set(NCBI_COMPONENT_PERL_INCLUDE ${PERL_INCLUDE_PATH})
        set(NCBI_COMPONENT_PERL_LIBS    ${PERL_LIBRARY})

        if(NCBI_TRACE_COMPONENT_PERL OR NCBI_TRACE_ALLCOMPONENTS)
            message("PERL: include dir = ${NCBI_COMPONENT_PERL_INCLUDE}")
            message("PERL: libs = ${NCBI_COMPONENT_PERL_LIBS}")
        endif()
    endif()
endif()
NCBIcomponent_report(PERL)

#############################################################################
# OpenSSL
NCBI_define_Xcomponent(NAME OpenSSL MODULE openssl PACKAGE OpenSSL LIB ssl crypto CHECK_INCLUDE openssl/ssl.h)
NCBIcomponent_report(OpenSSL)

#############################################################################
# MSGSL  (Microsoft Guidelines Support Library)
NCBI_define_Xcomponent(NAME MSGSL)
NCBIcomponent_report(MSGSL)

#############################################################################
# SGE  (Sun Grid Engine)
NCBI_define_Xcomponent(NAME SGE LIB drmaa LIBPATH_SUFFIX lib/lx-amd64)
NCBIcomponent_report(SGE)

#############################################################################
# MONGOCXX
NCBI_define_Xcomponent(NAME MONGOC LIB mongoc-1.0 bson-1.0)
if(NCBI_COMPONENT_MONGOC_FOUND)
    list(GET NCBI_COMPONENT_MONGOC_LIBS 0 NCBI_COMPONENT_MONGOC_LIBS_0)
    get_filename_component(NCBI_COMPONENT_MONGOC_LIBDIR ${NCBI_COMPONENT_MONGOC_LIBS_0} DIRECTORY)
    # Avoid find_*, as they return the usual versions, which may differ
    set(NCBI_COMPONENT_MONGOC_LIBSSL "${NCBI_COMPONENT_MONGOC_LIBDIR}/libssl.so")
    set(NCBI_COMPONENT_MONGOC_LIBCRYPTO "${NCBI_COMPONENT_MONGOC_LIBDIR}/libcrypto.so")
    if(EXISTS ${NCBI_COMPONENT_MONGOC_LIBSSL})
        list(APPEND NCBI_COMPONENT_MONGOC_LIBS ${NCBI_COMPONENT_MONGOC_LIBSSL})
    endif()
    if(EXISTS ${NCBI_COMPONENT_MONGOC_LIBCRYPTO})
        list(APPEND NCBI_COMPONENT_MONGOC_LIBS ${NCBI_COMPONENT_MONGOC_LIBCRYPTO})
    endif()
endif()
NCBI_define_Xcomponent(NAME MONGOCXX MODULE libmongocxx LIB mongocxx bsoncxx INCLUDE mongocxx/v_noabi bsoncxx/v_noabi)
NCBIcomponent_report(MONGOCXX)
NCBIcomponent_add(MONGOCXX MONGOC)

#############################################################################
# LEVELDB
# only has cmake cfg
NCBI_define_Xcomponent(NAME LEVELDB LIB leveldb)
NCBIcomponent_report(LEVELDB)

#############################################################################
# URing
NCBI_define_Xcomponent(NAME URing MODULE liburing LIB uring)
if(NCBI_COMPONENT_URing_FOUND AND NOT TARGET uring::uring)
    add_library(uring::uring UNKNOWN IMPORTED GLOBAL)
    set_target_properties(uring::uring PROPERTIES
        IMPORTED_LOCATION ${NCBI_COMPONENT_URing_LIBS}
        IMPORTED_LOCATION_DEBUG ${NCBI_COMPONENT_URing_LIBS}
        IMPORTED_LOCATION_RELEASE ${NCBI_COMPONENT_URing_LIBS}
    )
endif()
NCBIcomponent_report(URing)

#############################################################################
# ROCKSDB
NCBI_define_Xcomponent(NAME ROCKSDB CMAKE_PACKAGE RocksDB CMAKE_LIB rocksdb)
NCBIcomponent_report(ROCKSDB)

#############################################################################
# WGMLST
if(NOT NCBI_COMPONENT_WGMLST_DISABLED)
    find_package(SKESA)
    if (WGMLST_FOUND)
        set(NCBI_COMPONENT_WGMLST_FOUND YES)
        set(NCBI_COMPONENT_WGMLST_INCLUDE ${WGMLST_INCLUDE_DIRS})
        set(NCBI_COMPONENT_WGMLST_LIBS    ${WGMLST_LIBPATH} ${WGMLST_LIBRARIES})
    endif()
endif()
NCBIcomponent_report(WGMLST)

#############################################################################
# GLPK
NCBI_define_Xcomponent(NAME GLPK LIB glpk)
NCBIcomponent_report(GLPK)

#############################################################################
# UV
if(NOT NCBI_COMPONENT_UV_FOUND)
    NCBI_define_Xcomponent(NAME UV MODULE libuv LIB uv)
    if(NCBI_COMPONENT_UV_FOUND)
        set(NCBI_COMPONENT_UV_LIBS    ${NCBI_COMPONENT_UV_LIBS} ${CMAKE_THREAD_LIBS_INIT})
    endif()
endif()
NCBIcomponent_report(UV)

#############################################################################
# NGHTTP2
NCBI_define_Xcomponent(NAME NGHTTP2 MODULE libnghttp2 LIB nghttp2)
NCBIcomponent_report(NGHTTP2)

#############################################################################
# GL2PS
NCBI_define_Xcomponent(NAME GL2PS LIB gl2ps)
NCBIcomponent_report(GL2PS)

#############################################################################
# GMOCK
NCBI_define_Xcomponent(NAME GTEST MODULE gtest LIB gtest)
NCBI_define_Xcomponent(NAME GMOCK MODULE gmock LIB gmock ADD_COMPONENT GTEST)
NCBIcomponent_report(GMOCK)

#############################################################################
# CASSANDRA
NCBI_define_Xcomponent(NAME CASSANDRA LIB cassandra)
NCBIcomponent_report(CASSANDRA)

#############################################################################
# H2O
NCBI_define_Xcomponent(NAME H2O MODULE libh2o LIB h2o)
NCBIcomponent_report(H2O)

#############################################################################
# GMP
NCBI_define_Xcomponent(NAME GMP LIB gmp CHECK_INCLUDE gmp.h)
NCBIcomponent_report(GMP)

#############################################################################
# NETTLE
NCBI_define_Xcomponent(NAME NETTLE LIB hogweed nettle ADD_COMPONENT GMP CHECK_INCLUDE nettle/nettle-stdint.h)
NCBIcomponent_report(NETTLE)

#############################################################################
# GNUTLS
if(NOT NCBI_COMPONENT_GNUTLS_DISABLED)
    NCBI_find_Xlibrary(NCBI_COMPONENT_IDN_LIBS idn)
    if(NCBI_COMPONENT_IDN_LIBS)
        set(NCBI_COMPONENT_IDN_FOUND YES)
    endif()
    NCBI_define_Xcomponent(NAME GNUTLS LIB gnutls
      ADD_COMPONENT NETTLE IDN Z ZSTD)
endif()
NCBIcomponent_report(GNUTLS)

#############################################################################
# NCBICRYPT
NCBI_define_Xcomponent(NAME NCBICRYPT LIB ncbicrypt)
NCBIcomponent_report(NCBICRYPT)

#############################################################################
# THRIFT
NCBI_define_Xcomponent(NAME THRIFT LIB thrift
                        ADD_COMPONENT Boost.Test.Included)
NCBIcomponent_report(THRIFT)

#############################################################################
# NLohmann_JSON
NCBI_define_Xcomponent(NAME NLohmann_JSON CMAKE_PACKAGE nlohmann_json)
NCBIcomponent_report(NLohmann_JSON)

#############################################################################
# YAML_CPP
NCBI_define_Xcomponent(NAME YAML_CPP LIB yaml-cpp)
NCBIcomponent_report(YAML_CPP)

#############################################################################
# OPENTRACING
NCBI_define_Xcomponent(NAME OPENTRACING LIB opentracing)
NCBIcomponent_report(OPENTRACING)

#############################################################################
# JAEGER
NCBI_define_Xcomponent(NAME JAEGER LIB jaegertracing ADD_COMPONENT NLohmann_JSON OPENTRACING YAML_CPP THRIFT)
NCBIcomponent_report(JAEGER)

#############################################################################
# OPENTELEMETRY
NCBI_define_Xcomponent(NAME OPENTELEMETRY CMAKE_PACKAGE opentelemetry-cpp
  CMAKE_LIB otlp_grpc_exporter otlp_http_exporter ostream_span_exporter
            metrics)
NCBIcomponent_report(OPENTELEMETRY)

#############################################################################
# AWSSDK
if(NOT NCBI_COMPONENT_AWS_SDK_FOUND AND NOT NCBI_COMPONENT_Z_DISABLED)
    cmake_policy(PUSH)
    if(POLICY CMP0130)
        cmake_policy(SET CMP0130 OLD)
    endif()
    NCBI_define_Xcomponent(NAME AWS_SDK LIB
        aws-cpp-sdk-identity-management aws-cpp-sdk-access-management aws-cpp-sdk-transfer
        aws-cpp-sdk-sts aws-cpp-sdk-s3 aws-cpp-sdk-iam aws-cpp-sdk-cognito-identity
        aws-cpp-sdk-core aws-c-event-stream aws-checksums aws-c-common
        crypto ssl ADD_COMPONENT CURL
        CMAKE_PACKAGE AWSSDK COMPONENTS s3
    )
    if(NCBI_COMPONENT_AWS_SDK_FOUND AND NOT TARGET ZLIB::ZLIB)
        find_package(ZLIB REQUIRED)
    endif()
    cmake_policy(POP)
endif()
NCBIcomponent_report(AWS_SDK)

#############################################################################
# LIBSSH
NCBI_define_Xcomponent(NAME SSH LIB ssh)
NCBIcomponent_report(SSH)
