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


if(XCODE)
    set(NCBI_REQUIRE_XCODE_FOUND YES)
    list(APPEND NCBI_ALL_REQUIRES XCODE)
endif()

#to debug
#set(NCBI_TRACE_COMPONENT_JPEG ON)

#############################################################################
# common settings
set(NCBI_OPT_ROOT  /opt/X11)

############################################################################
# prebuilt libraries

set(NCBI_ThirdParty_BACKWARD   ${NCBI_TOOLS_ROOT}/backward-cpp-1.6-ncbi1 CACHE PATH "BACKWARD root")
set(NCBI_ThirdParty_LMDB       ${NCBI_TOOLS_ROOT}/lmdb-0.9.24 CACHE PATH "LMDB root")
set(NCBI_ThirdParty_LZO        ${NCBI_TOOLS_ROOT}/lzo-2.05 CACHE PATH "LZO root")
set(NCBI_ThirdParty_ZSTD          ${NCBI_TOOLS_ROOT}/zstd-1.5.2 CACHE PATH "ZSTD root")
set(NCBI_ThirdParty_SQLITE3    ${NCBI_TOOLS_ROOT}/sqlite-3.26.0-ncbi1 CACHE PATH "SQLITE2 root")
set(NCBI_ThirdParty_Boost_VERSION "1.76.0")
set(NCBI_ThirdParty_Boost      ${NCBI_TOOLS_ROOT}/boost-${NCBI_ThirdParty_Boost_VERSION}-ncbi1 CACHE PATH "Boost root")
set(NCBI_ThirdParty_BerkeleyDB ${NCBI_TOOLS_ROOT}/BerkeleyDB-5.3.28-ncbi1 CACHE PATH "BerkeleyDB root")
set(NCBI_ThirdParty_FASTCGI    ${NCBI_TOOLS_ROOT}/fcgi-2.4.0 CACHE PATH "FASTCGI root")
set(NCBI_ThirdParty_FASTCGI_SHLIB ${NCBI_ThirdParty_FASTCGI})
set(NCBI_ThirdParty_PYTHON_VERSION  3.9 CACHE STRING "PYTHON version")
#set(NCBI_ThirdParty_PYTHON     "/System/Library/Frameworks/Python.framework/Versions/${NCBI_ThirdParty_PYTHON_VERSION}" CACHE PATH "PYTHON root")
set(NCBI_ThirdParty_XCODE_FRAMEWORKS "/Applications/Xcode.app/Contents/Developer/Library/Frameworks")
set(NCBI_ThirdParty_PYTHON     "${NCBI_ThirdParty_XCODE_FRAMEWORKS}/Python3.framework/Versions/${NCBI_ThirdParty_PYTHON_VERSION}" CACHE PATH "PYTHON root")
#set(NCBI_ThirdParty_VDB        "/Volumes/trace_software/vdb/vdb-versions/cxx_toolkit/3")
set(NCBI_ThirdParty_VDB        "/net/snowman/vol/projects/trace_software/vdb/vdb-versions/cxx_toolkit/3" CACHE PATH "VDB root")
#set(NCBI_ThirdParty_XML        ${NCBI_TOOLS_ROOT}/libxml-2.7.8 CACHE PATH "XML root")
#set(NCBI_ThirdParty_XSLT       ${NCBI_TOOLS_ROOT}/libxml-2.7.8 CACHE PATH "XSLT root")
#set(NCBI_ThirdParty_EXSLT      ${NCBI_ThirdParty_XSLT})
set(NCBI_ThirdParty_FTGL      ${NCBI_TOOLS_ROOT}/ftgl-2.1.3-rc5 CACHE PATH "FTGL root")
set(NCBI_ThirdParty_GLEW      ${NCBI_TOOLS_ROOT}/glew-1.5.8 CACHE PATH "GLEW root")
set(NCBI_ThirdParty_GRPC      ${NCBI_TOOLS_ROOT}/grpc-1.50.2-ncbi1 CACHE PATH "GRPC root")
set(NCBI_ThirdParty_Boring    ${NCBI_ThirdParty_GRPC})
set(NCBI_ThirdParty_PROTOBUF  ${NCBI_TOOLS_ROOT}/grpc-1.50.2-ncbi1 CACHE PATH "PROTOBUF root")
set(NCBI_ThirdParty_wxWidgets ${NCBI_TOOLS_ROOT}/wxWidgets-3.2.1-ncbi1 CACHE PATH "wxWidgets root")
set(NCBI_ThirdParty_UV        ${NCBI_TOOLS_ROOT}/libuv-1.44.2 CACHE PATH "UV root")
set(NCBI_ThirdParty_NGHTTP2   ${NCBI_TOOLS_ROOT}/nghttp2-1.40.0 CACHE PATH "NGHTTP2 root")
set(NCBI_ThirdParty_GL2PS     ${NCBI_TOOLS_ROOT}/gl2ps-1.4.0 CACHE PATH "GL2PS root")
set(NCBI_ThirdParty_GMP       ${NCBI_TOOLS_ROOT}/gmp-6.3.0 CACHE PATH "GMP root")
set(NCBI_ThirdParty_NETTLE    ${NCBI_TOOLS_ROOT}/nettle-3.1.1 CACHE PATH "NETTLE root")
set(NCBI_ThirdParty_GNUTLS    ${NCBI_TOOLS_ROOT}/gnutls-3.4.0 CACHE PATH "GNUTLS root")
set(NCBI_ThirdParty_NCBICRYPT ${NCBI_TOOLS_ROOT}/ncbicrypt-20230516 CACHE PATH "NCBICRYPT root")

#############################################################################
#############################################################################

set(FOUNDATION_LIBS "-framework foundation")
set(COREFOUNDATION_LIBS "-framework CoreFoundation")

#############################################################################
# in-house-resources et al.
set(NCBI_REQUIRE_in-house-resources_FOUND NO)
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
if(NCBI_COMPONENT_BACKWARD_FOUND)
    set(HAVE_LIBBACKWARD_CPP YES)
    if(NOT NCBI_PTBCFG_USECONAN AND NOT NCBI_PTBCFG_HASCONAN AND NOT NCBI_PTBCFG_PACKAGING AND NOT NCBI_PTBCFG_PACKAGED)
        NCBI_find_system_library(LIBDW_LIBS NAMES dw)
        if (LIBDW_LIBS)
            set(HAVE_LIBDW 1)
            set(NCBI_COMPONENT_BACKWARD_LIBS ${NCBI_COMPONENT_BACKWARD_LIBS} ${LIBDW_LIBS})
        endif()
    endif()
endif()

#############################################################################
# Kerberos 5
if(NOT NCBI_COMPONENT_KRB5_DISABLED)
    set(NCBI_COMPONENT_KRB5_LIBS "-framework Kerberos")
    set(KRB5_LIBS ${NCBI_COMPONENT_KRB5_LIBS})
    set(NCBI_COMPONENT_KRB5_FOUND TRUE)
    set(HAVE_LIBKRB5 1)
endif()
NCBIcomponent_report(KRB5)

##############################################################################
# CURL
NCBI_define_Xcomponent(NAME CURL MODULE libcurl PACKAGE CURL LIB curl CHECK_INCLUDE curl/curl.h)
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
NCBI_define_Xcomponent(NAME PCRE LIB pcre CHECK_INCLUDE pcre.h)
set(NCBI_COMPONENT_PCRE2_FOUND NO)
NCBIcomponent_report(PCRE)
NCBIcomponent_report(PCRE2)
if(NOT NCBI_COMPONENT_PCRE_FOUND)
    if(EXISTS ${NCBITK_INC_ROOT}/util/regexp/pcre.h)
        set(NCBI_COMPONENT_PCRE_FOUND ${NCBI_COMPONENT_LocalPCRE_FOUND})
        set(NCBI_COMPONENT_PCRE_INCLUDE ${NCBI_COMPONENT_LocalPCRE_INCLUDE})
        set(NCBI_COMPONENT_PCRE_NCBILIB ${NCBI_COMPONENT_LocalPCRE_NCBILIB})
    else()
        set(NCBI_COMPONENT_PCRE2_FOUND ${NCBI_COMPONENT_LocalPCRE_FOUND})
        set(NCBI_COMPONENT_PCRE2_INCLUDE ${NCBI_COMPONENT_LocalPCRE_INCLUDE})
        set(NCBI_COMPONENT_PCRE2_NCBILIB ${NCBI_COMPONENT_LocalPCRE_NCBILIB})
    endif()
endif()
set(HAVE_LIBPCRE ${NCBI_COMPONENT_PCRE_FOUND})
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
#set(Boost_COMPILER "-clang-darwin100")
#include(${NCBI_TREE_CMAKECFG}/CMakeChecks.boost.cmake)

#############################################################################
# Boost.Test.Included
if(NOT NCBI_COMPONENT_Boost.Test.Included_FOUND)
    if (EXISTS ${NCBI_ThirdParty_Boost}/include)
        message(STATUS "Found Boost.Test.Included: ${NCBI_ThirdParty_Boost}")
        set(NCBI_COMPONENT_Boost.Test.Included_FOUND YES)
        set(NCBI_COMPONENT_Boost.Test.Included_INCLUDE ${NCBI_ThirdParty_Boost}/include)
        set(NCBI_COMPONENT_Boost.Test.Included_DEFINES BOOST_TEST_NO_LIB)
    else()
        NCBI_notice("Component Boost.Test.Included ERROR: ${NCBI_ThirdParty_Boost}/include not found")
        set(NCBI_COMPONENT_Boost.Test.Included_FOUND NO)
    endif()
endif()

#############################################################################
# Boost.Test
if(NOT NCBI_COMPONENT_Boost.Test_FOUND)
    NCBI_define_Xcomponent(NAME Boost.Test LIB boost_unit_test_framework-clang-darwin)
endif()

#############################################################################
# Boost.Spirit
if(NOT NCBI_COMPONENT_Boost.Spirit_FOUND)
    NCBI_define_Xcomponent(NAME Boost.Spirit LIB boost_thread-mt)
endif()

#############################################################################
# Boost.Thread
if(NOT NCBI_COMPONENT_Boost.Thread_FOUND)
    NCBI_define_Xcomponent(NAME Boost.Thread LIB boost_thread-mt)
endif()
endif(NOT NCBI_COMPONENT_Boost_DISABLED AND NOT NCBI_COMPONENT_Boost_FOUND)
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
# GIF
#NCBI_find_package(GIF GIF)
#set(NCBI_COMPONENT_GIF_FOUND YES)
#list(APPEND NCBI_ALL_COMPONENTS GIF)

#############################################################################
# TIFF
NCBI_define_Xcomponent(NAME TIFF MODULE libtiff-4 PACKAGE TIFF LIB tiff CHECK_INCLUDE tiffio.h)
NCBIcomponent_report(TIFF)

#############################################################################
# FASTCGI
NCBI_define_Xcomponent(NAME FASTCGI LIB fcgi)
NCBIcomponent_report(FASTCGI)

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
#BerkeleyDB
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
    set(ODBC_INCLUDE  ${NCBI_COMPONENT_XODBC_INCLUDE})
endif()
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
NCBI_define_Xcomponent(NAME PYTHON LIB python${NCBI_ThirdParty_PYTHON_VERSION} INCLUDE python${NCBI_ThirdParty_PYTHON_VERSION})
if(NCBI_COMPONENT_PYTHON_FOUND AND EXISTS ${NCBI_ThirdParty_XCODE_FRAMEWORKS})
    set(NCBI_COMPONENT_PYTHON_LIBS -Wl,-rpath,${NCBI_ThirdParty_XCODE_FRAMEWORKS} ${NCBI_COMPONENT_PYTHON_LIBS})
endif()
NCBIcomponent_report(PYTHON)

#############################################################################
# VDB
if(NOT NCBI_COMPONENT_VDB_DISABLED AND NOT NCBI_COMPONENT_VDB_FOUND)
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
        LIBPATH_SUFFIX mac/release/${NCBI_ThirdParty_VDB_ARCH}/lib INCPATH_SUFFIX interfaces)
    if(NCBI_COMPONENT_VDB_FOUND)
        set(NCBI_COMPONENT_VDB_INCLUDE
            ${NCBI_COMPONENT_VDB_INCLUDE} 
            ${NCBI_COMPONENT_VDB_INCLUDE}/os/mac
            ${NCBI_COMPONENT_VDB_INCLUDE}/os/unix
            ${NCBI_COMPONENT_VDB_INCLUDE}/cc/${NCBI_ThirdParty_VDB_COMPILER}/${NCBI_ThirdParty_VDB_ARCH}
            ${NCBI_COMPONENT_VDB_INCLUDE}/cc/${NCBI_ThirdParty_VDB_COMPILER}
        )
        set(NCBI_COMPONENT_VDB_LIBPATH ${NCBI_ThirdParty_VDB}/mac/release/${NCBI_ThirdParty_VDB_ARCH}/lib)
        set(HAVE_NCBI_VDB 1)
    endif()
endif()
NCBIcomponent_report(VDB)

#############################################################################
# wxWidgets
if(NOT NCBI_COMPONENT_wxWidgets_FOUND)
    set(_wx_ver 3.2)
    NCBI_define_Xcomponent(NAME wxWidgets LIB
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
endif()
NCBIcomponent_report(wxWidgets)

#############################################################################
# XML
if(NOT NCBI_COMPONENT_XML_FOUND)
    NCBI_define_Xcomponent(NAME XML MODULE libxml-2.0 PACKAGE LibXml2 LIB xml2 INCLUDE libxml2 CHECK_INCLUDE libxml/xmlexports.h)
    if(NCBI_COMPONENT_XML_FOUND)
        string(REPLACE ";" "?" _x "${NCBI_COMPONENT_XML_LIBS}")
        string(REPLACE "-L/sw/lib?" "" _x "${_x}")
        string(REPLACE "?" ";" NCBI_COMPONENT_XML_LIBS "${_x}")
    endif()
endif()
NCBIcomponent_report(XML)

#############################################################################
# XSLT
if(NOT NCBI_COMPONENT_XSLT_FOUND)
    NCBI_define_Xcomponent(NAME XSLT MODULE libxslt PACKAGE LibXslt LIB xslt ADD_COMPONENT XML CHECK_INCLUDE libxslt/xslt.h)
    if(NCBI_COMPONENT_XSLT_FOUND)
        string(REPLACE ";" "?" _x "${NCBI_COMPONENT_XSLT_LIBS}")
        string(REPLACE "-L/sw/lib?" "" _x "${_x}")
        string(REPLACE "?" ";" NCBI_COMPONENT_XSLT_LIBS "${_x}")
    endif()
endif()
if(NCBI_COMPONENT_XSLT_FOUND)
    if(NOT DEFINED NCBI_XSLTPROCTOOL)
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
NCBIcomponent_report(XSLT)

#############################################################################
# EXSLT
if(NOT NCBI_COMPONENT_EXSLT_FOUND)
    NCBI_define_Xcomponent(NAME EXSLT MODULE libexslt PACKAGE LibXslt LIB exslt ADD_COMPONENT XML GCRYPT CHECK_INCLUDE libexslt/exslt.h)
    if(NCBI_COMPONENT_EXSLT_FOUND)
        set(NCBI_COMPONENT_EXSLT_LIBS ${LIBXSLT_EXSLT_LIBRARIES} ${NCBI_COMPONENT_EXSLT_LIBS})

        string(REPLACE ";" "?" _x "${NCBI_COMPONENT_EXSLT_LIBS}")
        string(REPLACE "-L/sw/lib?" "" _x "${_x}")
        string(REPLACE "?" ";" NCBI_COMPONENT_EXSLT_LIBS "${_x}")
    endif()
endif()
NCBIcomponent_report(EXSLT)

#############################################################################
# LAPACK
NCBI_define_Xcomponent(NAME LAPACK PACKAGE LAPACK LIB lapack)
NCBIcomponent_report(LAPACK)

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
NCBI_define_Xcomponent(NAME GLEW MODULE glew LIB GLEW)
if(NCBI_COMPONENT_GLEW_FOUND)
    get_filename_component(_incdir "${NCBI_COMPONENT_GLEW_INCLUDE}" DIRECTORY)
    get_filename_component(_incGL "${NCBI_COMPONENT_GLEW_INCLUDE}" NAME)
    if("${_incGL}" STREQUAL "GL")
        set(NCBI_COMPONENT_GLEW_INCLUDE ${_incdir})
    endif()
endif()
NCBIcomponent_report(GLEW)

#############################################################################
# OpenGL
if(NOT NCBI_COMPONENT_OpenGL_DISABLED)
    set(NCBI_COMPONENT_OpenGL_FOUND YES)
    set(NCBI_COMPONENT_OpenGL_LIBS
      "-framework AGL -framework GLKit -framework OpenGL -framework Metal -framework MetalKit")
endif()
NCBIcomponent_report(OpenGL)

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
        grpc++ grpc address_sorting upb cares gpr re2
        absl_raw_hash_set absl_hashtablez_sampler absl_exponential_biased absl_hash 
        absl_city absl_statusor absl_bad_variant_access absl_status absl_cord 
        absl_str_format_internal absl_synchronization absl_graphcycles_internal 
        absl_symbolize absl_demangle_internal absl_stacktrace absl_debugging_internal 
        absl_malloc_internal absl_time absl_time_zone absl_civil_time absl_strings 
        absl_strings_internal absl_throw_delegate absl_int128 absl_base absl_spinlock_wait 
        absl_bad_optional_access absl_raw_logging_internal absl_log_severity
        )
    if(NCBI_COMPONENT_GRPC_FOUND)
        set(NCBI_COMPONENT_GRPC_LIBS ${NCBI_COMPONENT_GRPC_LIBS} ${NCBI_COMPONENT_Boring_LIBS})
        set(NCBI_COMPONENT_GRPC_LIBS ${NCBI_COMPONENT_GRPC_LIBS} ${COREFOUNDATION_LIBS})
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

#############################################################################
# UV
NCBI_define_Xcomponent(NAME UV MODULE libuv LIB uv)
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
# GMP
NCBI_define_Xcomponent(NAME GMP LIB gmp CHECK_INCLUDE gmp.h)
NCBIcomponent_report(GMP)

#############################################################################
# NETTLE
NCBI_define_Xcomponent(NAME NETTLE LIB hogweed nettle ADD_COMPONENT GMP CHECK_INCLUDE nettle/nettle-stdint.h)
NCBIcomponent_report(NETTLE)

#############################################################################
# GNUTLS
NCBI_define_Xcomponent(NAME GNUTLS LIB gnutls ADD_COMPONENT NETTLE)
NCBIcomponent_report(GNUTLS)

#############################################################################
# NCBICRYPT
NCBI_define_Xcomponent(NAME NCBICRYPT LIB ncbicrypt)
NCBIcomponent_report(NCBICRYPT)
