#############################################################################
# $Id$
#############################################################################

   NOT USED ANY MORE - use CMake.NCBIComponentsUNIXex.cmake instead

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


# NOTE:
#  for the time being, macros are defined at the end of the file
# ---------------------------------------------------------------------------

set(NCBI_ALL_COMPONENTS "")
#############################################################################


include(CheckLibraryExists)

check_library_exists(dl dlopen "" HAVE_LIBDL)
if(HAVE_LIBDL)
    set(DL_LIBS -ldl)
else(HAVE_LIBDL)
    if (UNIX)
        message(FATAL_ERROR "dl library not found")
    endif(UNIX)
endif(HAVE_LIBDL)

check_library_exists(jpeg jpeg_start_decompress "" HAVE_LIBJPEG)
check_library_exists(gif EGifCloseFile "" HAVE_LIBGIF)
check_library_exists(tiff TIFFClientOpen "" HAVE_LIBTIFF)
check_library_exists(png png_create_read_struct "" HAVE_LIBPNG)

option(USE_LOCAL_BZLIB "Use a local copy of libbz2")
option(USE_LOCAL_PCRE "Use a local copy of libpcre")



############################################################################
# NOTE:
# We conditionally set a package config path
# The existence of files in this directory switches find_package()
# to automatically switch between module mode vs. config mode
#
set(NCBI_TOOLS_ROOT $ENV{NCBI})
if (EXISTS ${NCBI_TOOLS_ROOT})
    set(_NCBI_DEFAULT_PACKAGE_SEARCH_PATH "${NCBI_TREE_CMAKECFG}/ncbi-defaults")
    set(CMAKE_PREFIX_PATH
        ${CMAKE_PREFIX_PATH}
        ${_NCBI_DEFAULT_PACKAGE_SEARCH_PATH}
        )
    message(STATUS "NCBI Root: ${NCBI_TOOLS_ROOT}")
    message(STATUS "CMake Prefix Path: ${CMAKE_PREFIX_PATH}")
else()
    message(STATUS "NCBI Root: <not found>")
endif()

if(WIN32)
    # Specific location overrides for Windows packages
    # Note: this variable must be set in the CMake GUI!!
    #set(WIN32_PACKAGE_ROOT "C:/Users/dicuccio/dev/packages")
    set(GIF_ROOT "${WIN32_PACKAGE_ROOT}/giflib-4.1.4-1-lib")
    set (ENV{GIF_DIR} "${GIF_ROOT}")

    set(PCRE_PKG_ROOT "${WIN32_PACKAGE_ROOT}/pcre-7.9-lib")
    set(PCRE_PKG_INCLUDE_DIRS "${PCRE_PKG_ROOT}/include")
    set(PCRE_PKG_LIBRARY_DIRS "${PCRE_PKG_ROOT}/lib")

    set(GNUTLS_ROOT "${WIN32_PACKAGE_ROOT}/gnutls-3.4.9")
    set(PC_GNUTLS_INCLUDEDIR "${GNUTLS_ROOT}/include")
    set(PC_GNUTLS_LIBDIR "${GNUTLS_ROOT}/lib")

    set(FREETYPE_ROOT "${WIN32_PACKAGE_ROOT}/freetype-2.3.5-1-lib")
    set(ENV{FREETYPE_DIR} "${FREETYPE_ROOT}")

    set(FTGL_ROOT "${WIN32_PACKAGE_ROOT}/ftgl-2.1.3_rc5")
    set(LZO_ROOT "${WIN32_PACKAGE_ROOT}/lzo-2.05-lib")

    set(CMAKE_PREFIX_PATH
        ${CMAKE_PREFIX_PATH}
        "${PCRE_PKG_ROOT}"
        "${GNUTLS_ROOT}"
        "${LZO_ROOT}"
        "${GIF_ROOT}"
        "${WIN32_PACKAGE_ROOT}/libxml2-2.7.8.win32"
        "${WIN32_PACKAGE_ROOT}/libxslt-1.1.26.win32"
        "${WIN32_PACKAGE_ROOT}/iconv-1.9.2.win32"
        "${FREETYPE_ROOT}"
        "${WIN32_PACKAGE_ROOT}/glew-1.5.8"
        "${FTGL_ROOT}"
        "${WIN32_PACKAGE_ROOT}/sqlite3-3.8.10.1"
        "${WIN32_PACKAGE_ROOT}/db-4.6.21"
		"${WIN32_PACKAGE_ROOT}/lmdb-0.9.18"
        )
endif()

#
# Framework for dealing with external libraries
#
include(${NCBI_TREE_CMAKECFG}/FindExternalLibrary.cmake)

############################################################################
#
# PCRE additions
#
include(${NCBI_TREE_CMAKECFG}/CMakeChecks.pcre.cmake)

############################################################################
#
# Compression libraries
include(${NCBI_TREE_CMAKECFG}/CMakeChecks.compress.cmake)

#################################
# Some platform-specific system libs that can be linked eventually
set(THREAD_LIBS   ${CMAKE_THREAD_LIBS_INIT})
if (WIN32)
    set(KSTAT_LIBS    "")
    set(RPCSVC_LIBS   "")
    set(DEMANGLE_LIBS "")
    set(UUID_LIBS      "")
    set(NETWORK_LIBS  "ws2_32.lib")
    set(RT_LIBS          "")
    set(MATH_LIBS      "")
    set(CURL_LIBS      "")
    set(MYSQL_INCLUDE_DIR      "")
endif()

find_library(UUID_LIBS NAMES uuid)
find_library(CRYPT_LIBS NAMES crypt)
find_library(MATH_LIBS NAMES m)

if (APPLE)
  find_library(NETWORK_LIBS c)
  find_library(RT_LIBS c)
else (APPLE)
  find_library(NETWORK_LIBS nsl)
  find_library(RT_LIBS        rt)
endif (APPLE)

#
# Basic Library Definitions
# FIXME: get rid of these
#

set(ORIG_LIBS   ${RT_LIBS} ${MATH_LIBS} ${CMAKE_THREAD_LIBS_INIT})
set(ORIG_C_LIBS            ${MATH_LIBS} ${CMAKE_THREAD_LIBS_INIT})
set(C_LIBS      ${ORIG_C_LIBS})


# ############################################################
# Specialized libs settings
# Mostly, from Makefile.mk
# ############################################################
#

set(LIBS ${LIBS} ${DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})


#
# NCBI-specific library subsets
# NOTE:
# these should be eliminated or simplified; they're not needed
#
set(LOCAL_LBSM ncbi_lbsm ncbi_lbsm_ipc ncbi_lbsmd)

############################################################################
#
# OS-specific settings
if (UNIX)
    find_library(GMP_LIB LIBS gmp HINTS "$ENV{NCBI}/gmp-6.0.0a/lib64/")
    find_library(IDN_LIB LIBS idn HINTS "/lib64")
    find_library(NETTLE_LIB LIBS nettle HINTS "$ENV{NCBI}/nettle-3.1.1/lib64")
    find_library(HOGWEED_LIB LIBS hogweed HINTS "$ENV{NCBI}/nettle-3.1.1/lib64")
elseif (WIN32)
    set(GMP_LIB "")
    set(IDN_LIB "")
    set(NETTLE_LIB "")
    set(HOGWEED_LIB "")
endif()

find_package(GnuTLS)
if (GnuTLS_FOUND)
    set(GNUTLS_LIBRARIES ${GNUTLS_LIBRARIES} ${ZLIB_LIBRARIES} ${IDN_LIB} ${RT_LIBS} ${HOGWEED_LIB} ${NETTLE_LIB} ${GMP_LIB})
    set(GNUTLS_LIBS ${GNUTLS_LIBRARIES})
endif()

############################################################################
#
# Kerberos 5 (via GSSAPI)
# FIXME: replace with native CMake check
#
find_external_library(KRB5 INCLUDES gssapi/gssapi_krb5.h LIBS gssapi_krb5 krb5 k5crypto com_err)


############################################################################
#
# Sybase stuff
#find_package(Sybase)

if (WIN32)
	find_external_library(Sybase
    DYNAMIC_ONLY
    INCLUDES sybdb.h
    #LIBS libsybblk
	LIBS libsybblk libsybdb64 libsybct libsybcs
    INCLUDE_HINTS "${WIN32_PACKAGE_ROOT}\\sybase-15.5\\include"
	LIBS_HINTS "${WIN32_PACKAGE_ROOT}\\sybase-15.5\\lib")
else (WIN32)
	find_external_library(Sybase
#    DYNAMIC_ONLY
    INCLUDES sybdb.h
    LIBS           sybblk_r64 sybct_r64 sybcs_r64 sybtcl_r64 sybcomn_r64 sybintl_r64 sybunic64
#    LIBS  sybdb64 sybblk_r64 sybct_r64 sybcs_r64 sybtcl_r64 sybcomn_r64 sybintl_r64 sybunic64
    HINTS "/opt/sybase/clients/15.7-64bit/OCS-15_0/")
endif (WIN32)

if (NOT SYBASE_FOUND)
	message(FATAL "no sybase found." )
endif (NOT SYBASE_FOUND)

set(SYBASE_DBLIBS "${SYBASE_LIBS}")

############################################################################
#
# FreeTDS
# FIXME: do we need these anymore?
#

set(ftds100   ftds100)
set(FTDS100_CTLIB_LIBS  ${ICONV_LIBS} ${KRB5_LIBS})
set(FTDS100_CTLIB_LIB   ct_ftds100 tds_ftds100)
set(FTDS100_CTLIB_INCLUDE ${includedir}/dbapi/driver/ftds100/freetds)
set(FTDS100_LIBS        ${FTDS100_CTLIB_LIBS})
set(FTDS100_LIB        ${FTDS100_CTLIB_LIB})
set(FTDS100_INCLUDE    ${FTDS100_CTLIB_INCLUDE})

set(ftds          ftds100)
set(FTDS_LIBS     ${FTDS100_LIBS})
set(FTDS_LIB      ${FTDS100_LIB})
set(FTDS_INCLUDE  ${FTDS100_INCLUDE})

#OpenSSL
find_package(OpenSSL)
if (OpenSSL_FOUND)
    set(OpenSSL_LIBRARIES ${OPENSSL_LIBRARIES} ${Z_LIBS} ${DL_LIBS})
    set(OPENSSL_LIBS ${OPENSSL_LIBRARIES})
    set(HAVE_LIBOPENSSL 1)
    message(STATUS "OpenSSL_LIBRARIES = ${OpenSSL_LIBRARIES}")
endif()

#EXTRALIBS were taken from mysql_config --libs
find_external_library(Mysql INCLUDES mysql/mysql.h LIBS mysqlclient EXTRALIBS ${Z_LIBS} ${CRYPT_LIBS} ${NETWORK_LIBS} ${MATH_LIBS} ${OPENSSL_LIBS})

############################################################################
#
# BerkeleyDB
include(${NCBI_TREE_CMAKECFG}/CMakeChecks.BerkeleyDB.cmake)

# ODBC
# FIXME: replace with native CMake check
#
set(ODBC_INCLUDE  ${includedir}/dbapi/driver/odbc/unix_odbc 
                  ${includedir0}/dbapi/driver/odbc/unix_odbc)
set(ODBC_LIBS)

# Python
find_external_library(Python
    INCLUDES Python.h
    LIBS python2.7
    INCLUDE_HINTS "/opt/python-2.7env/include/python2.7/"
    LIBS_HINTS "/opt/python-2.7env/lib/")

############################################################################
#
# Boost settings
include(${NCBI_TREE_CMAKECFG}/CMakeChecks.boost.cmake)

############################################################################
#
# NCBI C Toolkit:  headers and libs
# Path overridden in stable components to avoid version skew.
set(NCBI_C_ROOT "${NCBI_TOOLS_ROOT}/ncbi")
string(REGEX MATCH "DNCBI_INT8_GI|NCBI_STRICT_GI" INT8GI_FOUND "${CMAKE_CXX_FLAGS}")
if (NOT "${INT8GI_FOUND}" STREQUAL "")
    if (EXISTS "${NCBI_C_ROOT}/ncbi.gi64/")
        set(NCBI_CTOOLKIT_PATH "${NCBI_C_ROOT}/ncbi.gi64/")
    elseif (EXISTS "${NCBI_C_ROOT}.gi64")
        set(NCBI_CTOOLKIT_PATH "${NCBI_C_ROOT}.gi64/")
    else ()
        set(NCBI_CTOOLKIT_PATH "${NCBI_C_ROOT}/")
    endif ()
else ()
    set(NCBI_CTOOLKIT_PATH "${NCBI_C_ROOT}/")
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
    set(HAVE_NCBI_C true)
elseif (WIN32)
    set(NCBI_CTOOLKIT_WIN32 "//snowman/win-coremake/Lib/Ncbi/C_Toolkit/vs2015.64/c.current")
    if (EXISTS "${NCBI_CTOOLKIT_WIN32}")
        set(NCBI_C_INCLUDE "${NCBI_CTOOLKIT_WIN32}/include")
        set(NCBI_C_LIBPATH "${NCBI_CTOOLKIT_WIN32}/lib")
        set(NCBI_C_ncbi    "ncbi.lib")
        set(HAVE_NCBI_C true)
    else()
        set(HAVE_NCBI_C false)
    endif()
else ()
    set(HAVE_NCBI_C false)
endif ()

message(STATUS "HAVE_NCBI_C = ${HAVE_NCBI_C}")
message(STATUS "NCBI_C_INCLUDE = ${NCBI_C_INCLUDE}")
message(STATUS "NCBI_C_LIBPATH = ${NCBI_C_LIBPATH}")

############################################################################
#
# OpenGL: headers and libs (including core X dependencies) for code
# not using other toolkits.  (The wxWidgets variables already include
# these as appropriate.)

find_package(OpenGL)
if (WIN32)
    set(GLEW_INCLUDE ${WIN32_PACKAGE_ROOT}/glew-1.5.8/include)
    set(GLEW_LIBRARIES ${WIN32_PACKAGE_ROOT}/glew-1.5.8/lib/glew32mx.lib)
else()
    find_package(GLEW)
endif()
find_package(OSMesa)
if (${OPENGL_FOUND})
    set(OPENGL_INCLUDE "${OPENGL_INCLUDE_DIR}")
    set(OPENGL_LIBS "${OPENGL_LIBRARIES}")

    set(OPENGL_INCLUDE ${OPENGL_INCLUDE_DIRS})
    set(OPENGL_LIBS ${OPENGL_LIBRARIES})
    set(GLEW_INCLUDE ${GLEW_INCLUDE_DIRS})
    set(GLEW_LIBS ${GLEW_LIBRARIES})
    set(OSMESA_INCLUDE ${OSMesa_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS})
    set(OSMESA_LIBS ${OSMesa_LIBRARIES} ${OPENGL_LIBRARIES})

endif()

############################################################################
#
# wxWidgets
include(${NCBI_TREE_CMAKECFG}/CMakeChecks.wxwidgets.cmake)

# Fast-CGI
set(_fcgi_version "fcgi-2.4.0")
if (APPLE)
  find_external_library(FastCGI
      INCLUDES fastcgi.h
      LIBS fcgi
      HINTS "${NCBI_TOOLS_ROOT}/${_fcgi_version}")
else ()
    set(_fcgi_root "${NCBI_TOOLS_ROOT}/${_fcgi_version}")
    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release"
            AND BUILD_SHARED_LIBS 
            AND EXISTS /opt/ncbi/64/${_fcgi_version} )
        set(_fcgi_root "/opt/ncbi/64/${_fcgi_version}")
    endif()

    if (BUILD_SHARED_LIBS)
        set(_fcgi_root "${_fcgi_root}/shlib")
    else()
        set(_fcgi_root "${_fcgi_root}/lib")
    endif()

    find_library(FASTCGI_LIBS
        NAMES fcgi
        HINTS "${_fcgi_root}")
    find_path(FASTCGI_INCLUDE
        NAME fastcgi.h
        HINTS "${NCBI_TOOLS_ROOT}/${_fcgi_version}/include")

    if (FASTCGI_LIBS)
        set(FASTCGI_FOUND True)
    endif()

    message(STATUS "FastCGI found at: ${FASTCGI_LIBS}")
endif()

# Fast-CGI lib:  (module to add to the "xcgi" library)
set(FASTCGI_OBJS    fcgibuf)

# NCBI SSS:  headers, library path, libraries
set(NCBI_SSS_INCLUDE ${incdir}/sss)
set(NCBI_SSS_LIBPATH )
set(LIBSSSUTILS      -lsssutils)
set(LIBSSSDB         -lsssdb -lssssys)
set(sssutils         sssutils)
set(NCBILS2_LIB      ncbils2_cli ncbils2_asn ncbils2_cmn)
set(NCBILS_LIB       ${NCBILS2_LIB})


# SP:  headers, libraries
set(SP_INCLUDE )
set(SP_LIBS    )

# ORBacus CORBA headers, library path, libraries
set(ORBACUS_INCLUDE )
set(ORBACUS_LIBPATH )
set(LIBOB           )
# LIBIMR should be empty for single-threaded builds
set(LIBIMR          )

# IBM's International Components for Unicode
find_external_library(ICU
    INCLUDES unicode/ucnv.h
    LIBS icui18n icuuc icudata
    HINTS "${NCBI_TOOLS_ROOT}/icu-49.1.1")


##############################################################################
## LibXml2 / LibXsl
##
find_library(GCRYPT_LIBS NAMES gcrypt HINTS "$ENV{NCBI}/libgcrypt/${CMAKE_BUILD_TYPE}/lib")
find_library(GPG_ERROR_LIBS NAMES gpg-error HINTS "$ENV{NCBI}/libgpg-error/${CMAKE_BUILD_TYPE}/lib")
if (NOT GCRYPT_LIBS STREQUAL "GCRYPT_LIBS-NOTFOUND")
    set(GCRYPT_FOUND True)
    set(GCRYPT_LIBS ${GCRYPT_LIBS} ${GPG_ERROR_LIBS})
else()
    set(GCRYPT_FOUND False)
endif()

# ICONV
# Windows requires special handling for this
if (WIN32)
    find_external_library(iconv
        INCLUDES iconv.h
        LIBS iconv)
endif()

find_package(LibXml2)
if (LIBXML2_FOUND)
    set(LIBXML_INCLUDE ${LIBXML2_INCLUDE_DIR})
    set(LIBXML_LIBS    ${LIBXML2_LIBRARIES})
    if (WIN32)
        set(LIBXML_INCLUDE ${LIBXML_INCLUDE} ${ICONV_INCLUDE})
        set(LIBXML_LIBS ${LIBXML_LIBS} ${ICONV_LIBS})
    endif()
endif()

find_package(LibXslt)
if (LIBXSLT_FOUND)
    set(LIBXSLT_INCLUDE  ${LIBXSLT_INCLUDE_DIR})
    set(LIBXSLT_LIBS     ${LIBXSLT_EXSLT_LIBRARIES} ${LIBXSLT_LIBRARIES} ${LIBXML_LIBS})
    set(LIBEXSLT_INCLUDE ${LIBXSLT_INCLUDE_DIR})
    set(LIBEXSLT_LIBS    ${LIBXSLT_EXSLT_LIBRARIES})
    if (GCRYPT_FOUND)
        set(LIBXSLT_LIBS     ${LIBXSLT_LIBS} ${GCRYPT_LIBS})
        set(LIBEXSLT_LIBS    ${LIBEXSLT_LIBS} ${GCRYPT_LIBS})
    endif()
endif()


find_external_library(xerces
    INCLUDES xercesc/dom/DOM.hpp
    LIBS xerces-c
    HINTS "${NCBI_TOOLS_ROOT}/xerces-3.1.2/${CMAKE_BUILD_TYPE}")

find_external_library(xalan
    INCLUDES xalanc/XalanTransformer/XalanTransformer.hpp
    LIBS xalan-c xalanMsg
    HINTS "${NCBI_TOOLS_ROOT}/xalan-1.11/${CMAKE_BUILD_TYPE}")

# Sun Grid Engine (libdrmaa):
# libpath - /netmnt/uge/lib/lx-amd64/
find_external_library(SGE
    INCLUDES drmaa.h
    LIBS drmaa
    INCLUDE_HINTS "/netmnt/gridengine/current/include"
    LIBS_HINTS "/netmnt/gridengine/current/lib/lx-amd64/")

# muParser
find_external_library(muparser
    INCLUDES muParser.h
    LIBS muparser
    INCLUDE_HINTS "${NCBI_TOOLS_ROOT}/muParser-1.30/include"
    LIBS_HINTS "${NCBI_TOOLS_ROOT}/muParser-1.30/${CMAKE_BUILD_TYPE}/lib/")

# HDF5
find_external_library(hdf5
    INCLUDES hdf5.h
    LIBS hdf5
    HINTS "${NCBI_TOOLS_ROOT}/hdf5-1.8.3/${CMAKE_BUILD_TYPE}")

############################################################################
#
# SQLite3
include(${NCBI_TREE_CMAKECFG}/CMakeChecks.sqlite3.cmake)

############################################################################
#
# Various image-format libraries
include(${NCBI_TREE_CMAKECFG}/CMakeChecks.image.cmake)

#############################################################################
## MongoDB

find_library(SASL2_LIBS sasl2)

if (NOT WIN32)
    find_package(MongoCXX)
endif()
if (MONGOCXX_FOUND)
    set(MONGOCXX_INCLUDE ${MONGOCXX_INCLUDE_DIRS} ${BSONCXX_INCLUDE_DIRS})
    set(MONGOCXX_LIB ${MONGOCXX_LIBPATH} ${MONGOCXX_LIBRARIES} ${BSONCXX_LIBRARIES} ${OPENSSL_LIBS} ${SASL2_LIBS})
endif()
message(STATUS "MongoCXX Includes: ${MONGOCXX_INCLUDE}")

##############################################################################
##
## LevelDB

find_package(LEVELDB)

############################################################################
##
## wgMLST

if (NOT WIN32)
    find_package(SKESA)
endif()
if (WGMLST_FOUND)
    set(WGMLST_INCLUDE ${WGMLST_INCLUDE_DIRS})
    set(WGMLST_LIB ${WGMLST_LIBPATH} ${WGMLST_LIBRARIES})
endif()
message(STATUS "wgMLST Includes: ${WGMLST_INCLUDE}")

# libmagic (file-format identification)
find_library(MAGIC_LIBS magic)

# libcurl (URL retrieval)
find_library(CURL_LIBS curl)

# libmimetic (MIME handling)
find_external_library(mimetic
    INCLUDES mimetic/mimetic.h
    LIBS mimetic
    HINTS "${NCBI_TOOLS_ROOT}/mimetic-0.9.7-ncbi1/")

# libgSOAP++
find_external_library(gsoap
    INCLUDES stdsoap2.h
    LIBS gsoapssl++
    INCLUDE_HINTS "${NCBI_TOOLS_ROOT}/gsoap-2.8.15/include"
    LIBS_HINTS "${NCBI_TOOLS_ROOT}/gsoap-2.8.15/${CMAKE_BUILD_TYPE}/lib/"
    EXTRALIBS ${Z_LIBS})

set(GSOAP_SOAPCPP2 ${NCBI_TOOLS_ROOT}/gsoap-2.8.15/${CMAKE_BUILD_TYPE}/bin/soapcpp2)
set(GSOAP_WSDL2H   ${NCBI_TOOLS_ROOT}/gsoap-2.8.15/${CMAKE_BUILD_TYPE}/bin/wsdl2h)

# Compress
set(COMPRESS_LDEP ${CMPRS_LIB})
set(COMPRESS_LIBS xcompress ${COMPRESS_LDEP})


#################################
# Useful sets of object libraries
#################################


set(SOBJMGR_LIBS xobjmgr)
set(ncbi_xreader_pubseqos ncbi_xreader_pubseqos)
set(ncbi_xreader_pubseqos2 ncbi_xreader_pubseqos2)
set(OBJMGR_LIBS ncbi_xloader_genbank)


# Overlapping with qall is poor, so we have a second macro to make it
# easier to stay out of trouble.
set(QOBJMGR_ONLY_LIBS xobjmgr id2 seqsplit id1 genome_collection seqset
    ${SEQ_LIBS} pub medline biblio general-lib xcompress ${CMPRS_LIB})
set(QOBJMGR_LIBS ${QOBJMGR_ONLY_LIBS} qall)
set(QOBJMGR_STATIC_LIBS ${QOBJMGR_ONLY_LIBS} qall)

# EUtils
set(EUTILS_LIBS eutils egquery elink epost esearch espell esummary linkout
              einfo uilist ehistory)


#
# SRA/VDB stuff
if (WIN32)
	find_external_library(VDB
		INCLUDES sra/sradb.h
		LIBS ncbi-vdb
		INCLUDE_HINTS "\\\\snowman\\trace_software\\vdb\\vdb-versions\\2.11.0\\interfaces"
		LIBS_HINTS "\\\\snowman\\trace_software\\vdb\\vdb-versions\\2.11.0\\win\\release\\x86_64\\lib")
else (WIN32)
	find_external_library(VDB
		INCLUDES sra/sradb.h
		LIBS ncbi-vdb
 	    INCLUDE_HINTS "/opt/ncbi/64/trace_software/vdb/vdb-versions/cxx_toolkit/2/interfaces"
 	    LIBS_HINTS "/opt/ncbi/64/trace_software/vdb/vdb-versions/cxx_toolkit/2/linux/release/x86_64/lib/")
endif (WIN32)

if (VDB_FOUND)
	if (WIN32)
		set(VDB_INCLUDE "${VDB_INCLUDE}" "${VDB_INCLUDE}\\os\\win" "${VDB_INCLUDE}\\cc\\vc++\\x86_64" "${VDB_INCLUDE}\\cc\\vc++")
	else (WIN32)
		set(VDB_INCLUDE "${VDB_INCLUDE}" "${VDB_INCLUDE}/os/linux" "${VDB_INCLUDE}/os/unix" "${VDB_INCLUDE}/cc/gcc/x86_64" "${VDB_INCLUDE}/cc/gcc")
	endif(WIN32)
    set(SRA_INCLUDE ${VDB_INCLUDE})
    set(SRA_SDK_SYSLIBS ${VDB_LIBS})
    set(SRA_SDK_LIBS ${VDB_LIBS})
    set(SRAXF_LIBS ${SRA_SDK_LIBS})
    set(SRA_LIBS ${SRA_SDK_LIBS})
    set(BAM_LIBS ${SRA_SDK_LIBS})
    set(SRAREAD_LDEP ${SRA_SDK_LIBS})
    set(SRAREAD_LIBS sraread ${SRA_SDK_LIBS})
    set(HAVE_NCBI_VDB 1)
endif ()

# Makefile.blast_macros.mk
set(BLAST_DB_DATA_LOADER_LIBS ncbi_xloader_blastdb ncbi_xloader_blastdb_rmt)
set(BLAST_FORMATTER_MINIMAL_LIBS xblastformat align_format taxon1 blastdb_format
    gene_info xalnmgr blastxml xcgi xhtml)
set(BLAST_INPUT_LIBS blastinput
    ${BLAST_DB_DATA_LOADER_LIBS} ${BLAST_FORMATTER_MINIMAL_LIBS})

set(BLAST_FORMATTER_LIBS ${BLAST_INPUT_LIBS})

# Libraries required to link against the internal BLAST SRA library
set(BLAST_SRA_LIBS blast_sra ${SRAXF_LIBS} vxf ${SRA_LIBS})

# BLAST_FORMATTER_LIBS and BLAST_INPUT_LIBS need $BLAST_LIBS
set(BLAST_LIBS xblast xalgoblastdbindex composition_adjustment
xalgodustmask xalgowinmask seqmasks_io seqdb blast_services xobjutil
${OBJREAD_LIBS} xnetblastcli xnetblast blastdb scoremat tables xalnmgr)



# SDBAPI stuff
set(SDBAPI_LIB sdbapi) # ncbi_xdbapi_ftds ${FTDS_LIB} dbapi dbapi_driver ${XCONNEXT})


set(VARSVC_LIBS varsvcutil varsvccli varsvcobj)


# Entrez Libs
set(ENTREZ_LIBS entrez2cli entrez2)
set(EUTILS_LIBS eutils egquery elink epost esearch espell esummary linkout einfo uilist ehistory)

#GLPK
find_external_library(glpk
    INCLUDES glpk.h
    LIBS glpk
    HINTS "/usr/local/glpk/4.45")

find_external_library(samtools
    INCLUDES bam.h
    LIBS bam
    HINTS "${NCBI_TOOLS_ROOT}/samtools")

# libbackward
find_path(LIBBACKWARD_INCLUDE_DIR
    backward.hpp
    HINTS $ENV{NCBI}/backward-cpp-1.3/include)
if (LIBBACKWARD_INCLUDE_DIR)
    message(STATUS "Found libbackward: ${LIBBACKWARD_INCLUDE_DIR}")
    set(HAVE_LIBBACKWARD_CPP 1)
endif()

# libdw
find_library(LIBDW_LIBRARIES NAMES libdw.so HINTS /usr/lib64)
if (LIBDW_LIBRARIES)
    set(HAVE_LIBDW 1)
    message(STATUS "Found libdw: ${LIBDW_LIBRARIES}")
endif()


#LAPACK
check_include_file(lapacke.h HAVE_LAPACKE_H)
check_include_file(lapacke/lapacke.h HAVE_LAPACKE_LAPACKE_H)
check_include_file(Accelerate/Accelerate.h HAVE_ACCELERATE_ACCELERATE_H)
#find_external_library(LAPACK LIBS lapack blas)
find_package(LAPACK)
if (LAPACK_FOUND)
    set(LAPACK_INCLUDE ${LAPACK_INCLUDE_DIRS})
    set(LAPACK_LIBS ${LAPACK_LIBRARIES})
else ()
    find_library(LAPACK_LIBS lapack blas)
endif ()

#LMDB
if (WIN32)
	find_external_library(LMDB
		STATIC_ONLY
		INCLUDES lmdb.h 
		LIBS liblmdb 
		INCLUDE_HINTS "${WIN32_PACKAGE_ROOT}\\lmdb-0.9.18\\include" 
		LIB_HINTS "${WIN32_PACKAGE_ROOT}\\lmdb-0.9.18\\lib")
else (WIN32)
	find_external_library(LMDB INCLUDES lmdb.h LIBS lmdb HINTS "${NCBI_TOOLS_ROOT}/lmdb-0.9.18" EXTRALIBS pthread)
endif (WIN32)

if (LMDB_INCLUDE)
    set(HAVE_LIBLMDB 1)
endif()

############################################################################
##
## libxlsxwriter

set(_xlsxwriter_version "libxlsxwriter-0.6.9")
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release"
        AND BUILD_SHARED_LIBS 
        AND EXISTS /opt/ncbi/64/${_xlsxwriter_version}/lib/)
    set(_xlsxwriter_hints "/opt/ncbi/64/${_xlsxwriter_version}")
else()
    set(_xlsxwriter_hints "${NCBI_TOOLS_ROOT}/${_xlsxwriter_version}")
endif()

find_library(LIBXLSXWRITER_LIBS
    NAMES xlsxwriter
    HINTS "${_xlsxwriter_hints}/lib")
if (LIBXLSXWRITER_LIBS)
    find_path(LIBXLSXWRITER_INCLUDE
        NAMES xlsxwriter.h
        HINTS "${NCBI_TOOLS_ROOT}/${_xlsxwriter_version}/include")
    if (LIBXLSXWRITER_INCLUDE)
        set(LIBXLSXWRITER_FOUND True)
    endif()
endif()
if (LIBXLSXWRITER_FOUND)
    message(STATUS "Found libxlsxwriter: ${LIBXLSXWRITER_LIBS}")
else()
    message(STATUS "Could not find libxlsxwriter")
endif()

############################################################################
##
## libunwind

set(_unwind_version "libunwind-1.1")
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release"
        AND BUILD_SHARED_LIBS 
        AND EXISTS /opt/ncbi/64/${_unwind_version}/lib/)
    set(_unwind_hints "/opt/ncbi/64/${_unwind_version}")
else()
    set(_unwind_hints "${NCBI_TOOLS_ROOT}/${_unwind_version}")
endif()

find_library(LIBUNWIND_LIBS
    NAMES unwind
    HINTS "${_unwind_hints}/lib"
    )
if (LIBUNWIND_LIBS)
    find_path(LIBUNWIND_INCLUDE
        NAME libunwind.h
        HINTS "${NCBI_TOOLS_ROOT}/${_unwind_version}/include")
    if (LIBUNWIND_INCLUDE)
        set(HAVE_LIBUNWIND True)
    endif()
endif()

if (HAVE_LIBUNWIND)
    message(STATUS "Found libunwind: ${LIBUNWIND_LIBS}")
    message(STATUS "Found libunwind.h: ${LIBUNWIND_INCLUDE}")
else()
    message(STATUS "Cannot find libunwind")
endif()

if (EXISTS "${NCBI_TOOLS_ROOT}/msgsl-0.0.20171114-1c95f94/include/gsl/gsl")
    set(MSGSL_INCLUDE "${NCBI_TOOLS_ROOT}/msgsl-0.0.20171114-1c95f94/include/")
else()
    find_path(MSGSL_INCLUDE NAMES gsl/gsl)
endif()

if (NOT "${MSGSL_INCLUDE}" STREQUAL "")
    set(HAVE_MSGSL true)
    message(STATUS "Found MSGSL: ${MSGSL_INCLUDE}")
else()
    message(STATUS "Could NOT find MSGSL")
endif()

# temporarily include the standard python path when searching for Python3
set(CMAKE_PREFIX_PATH_ORIG ${CMAKE_PREFIX_PATH})
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} /opt/python-all/)

find_package(PythonInterp 3)
if (PYTHONINTERP_FOUND)
    if (NOT "$ENV{TEAMCITY_VERSION}" STREQUAL "")
        message(STATUS "Generating ${build_root}/run_with_cd_reporter.py...")
        message(STATUS "Python3 path: ${PYTHON_EXECUTABLE}")

        set(PYTHON3 ${PYTHON_EXECUTABLE})
        set(CD_REPORTER "/am/ncbiapdata/bin/cd_reporter")
        if (DEFINED NCBI_EXTERNAL_TREE_ROOT)
            set(abs_top_srcdir ${NCBI_EXTERNAL_TREE_ROOT})
        else()
            set(abs_top_srcdir ${abs_top_src_dir})
        endif()
        configure_file(${NCBI_TREE_BUILDCFG}/run_with_cd_reporter.py.in ${build_root}/build-system/run_with_cd_reporter.py)

        # copy to build_root and set executable permissions (workaround because configure_file doesn't set permissions)
        file(COPY ${build_root}/build-system/run_with_cd_reporter.py DESTINATION ${build_root}
            FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${build_root}/run_with_cd_reporter.py)
    else()
        message(STATUS "Detected development build, cd_reporter disabled")
    endif()
else(PYTHONINTERP_FOUND)
    message(STATUS "Could not find Python3. Disabling cd_reporter.")
endif(PYTHONINTERP_FOUND)
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH_ORIG})


if (WIN32)
	set(win_include_directories 
		"${PCRE_PKG_INCLUDE_DIRS}" 
		"${PC_GNUTLS_INCLUDEDIR}"
        "${WIN32_PACKAGE_ROOT}/sqlite3-3.8.10.1/include"
		"${WIN32_PACKAGE_ROOT}/db-4.6.21/include"
        "${WIN32_PACKAGE_ROOT}/libxml2-2.7.8.win32/include"
        "${WIN32_PACKAGE_ROOT}/libxslt-1.1.26.win32/include"
        "${WIN32_PACKAGE_ROOT}/iconv-1.9.2.win32/include"
		"${WIN32_PACKAGE_ROOT}/lmdb-0.9.18/include" )
	include_directories(${win_include_directories})
endif (WIN32)

#############################################################################
#############################################################################
#############################################################################
##
##          NCBI CMake wrapper adapter
##
#############################################################################
#############################################################################

set(NCBI_COMPONENT_unix_FOUND YES)
set(NCBI_COMPONENT_Linux_FOUND YES)

#############################################################################
# NCBI_C
if(HAVE_NCBI_C)
  message("NCBI_C found at ${NCBI_C_INCLUDE}")
  set(NCBI_COMPONENT_NCBI_C_FOUND YES)
  set(NCBI_COMPONENT_NCBI_C_INCLUDE ${NCBI_C_INCLUDE})

  set(_c_libs  ncbiobj ncbimmdb ncbi)
if (OFF)
  foreach( _lib IN LISTS _c_libs)
    set(NCBI_COMPONENT_NCBI_C_LIBS ${NCBI_COMPONENT_NCBI_C_LIBS} "${NCBI_C_LIBPATH}/lib${_lib}.a")
  endforeach()
else()
  set(NCBI_COMPONENT_NCBI_C_LIBS -L${NCBI_C_LIBPATH} ${_c_libs})
endif()

  set(NCBI_COMPONENT_NCBI_C_DEFINES HAVE_NCBI_C=1)
else()
  set(NCBI_COMPONENT_NCBI_C_FOUND NO)
endif()

#############################################################################
# STACKTRACE
set(NCBI_COMPONENT_STACKTRACE_FOUND YES)
set(NCBI_COMPONENT_STACKTRACE_INCLUDE ${LIBBACKWARD_INCLUDE_DIR} ${LIBUNWIND_INCLUDE})
set(NCBI_COMPONENT_STACKTRACE_LIBS ${LIBUNWIND_LIBS} ${LIBDW_LIBRARIES})

##############################################################################
# UUID
if (NOT UUID_LIBS-NOTFOUND)
set(NCBI_COMPONENT_UUID_FOUND YES)
set(NCBI_COMPONENT_UUID_LIBS ${UUID_LIBS})
endif()

##############################################################################
# CURL
if (NOT CURL_LIBS-NOTFOUND)
set(NCBI_COMPONENT_CURL_FOUND YES)
set(NCBI_COMPONENT_CURL_LIBS ${CURL_LIBS})
endif()

#############################################################################
# TLS
if(GnuTLS_FOUND)
  message("TLS found at ${GNUTLS_INCLUDE}")
  set(NCBI_COMPONENT_TLS_FOUND YES)
  set(NCBI_COMPONENT_TLS_INCLUDE ${GNUTLS_INCLUDE})
  set(NCBI_COMPONENT_TLS_LIBS ${GNUTLS_LIBS})
  set(HAVE_LIBGNUTLS 1)
else()
  set(NCBI_COMPONENT_TLS_FOUND NO)
endif()

#############################################################################
# FASTCGI
if(FASTCGI_FOUND)
  message(STATUS "FASTCGI found at ${FASTCGI_INCLUDE}")
  set(NCBI_COMPONENT_FASTCGI_FOUND YES)
  set(NCBI_COMPONENT_FASTCGI_INCLUDE ${FASTCGI_INCLUDE})
  set(NCBI_COMPONENT_FASTCGI_LIBS ${FASTCGI_LIBS})
  set(HAVE_LIBFASTCGI 1)
else()
    message(STATUS "FASTCGI not found")
  set(NCBI_COMPONENT_FASTCGI_FOUND NO)
endif()

#############################################################################
# Boost.Test.Included
if(Boost_FOUND)
  set(NCBI_COMPONENT_Boost.Test.Included_FOUND YES)
  set(NCBI_COMPONENT_Boost.Test.Included_INCLUDE ${Boost_INCLUDE_DIRS})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} Boost.Test.Included")
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
else()
  set(NCBI_COMPONENT_Boost_FOUND NO)
endif()

#############################################################################
# PCRE
if(PCRE_FOUND AND NOT USE_LOCAL_PCRE)
  set(NCBI_COMPONENT_PCRE_FOUND YES)
  set(NCBI_COMPONENT_PCRE_INCLUDE ${PCRE_INCLUDE_DIR})
  set(NCBI_COMPONENT_PCRE_LIBS ${PCRE_LIBRARIES})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} PCRE")
else()
  set(NCBI_COMPONENT_PCRE_FOUND ${NCBI_COMPONENT_LocalPCRE_FOUND})
  set(NCBI_COMPONENT_PCRE_INCLUDE ${NCBI_COMPONENT_LocalPCRE_INCLUDE})
  set(NCBI_COMPONENT_PCRE_NCBILIB ${NCBI_COMPONENT_LocalPCRE_NCBILIB})
endif()

#############################################################################
# Z
if(ZLIB_FOUND)
  set(NCBI_COMPONENT_Z_FOUND YES)
  set(NCBI_COMPONENT_Z_INCLUDE ${ZLIB_INCLUDE_DIR})
  set(NCBI_COMPONENT_Z_LIBS ${ZLIB_LIBRARIES})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} Z")
else()
  set(NCBI_COMPONENT_Z_FOUND ${NCBI_COMPONENT_LocalZ_FOUND})
  set(NCBI_COMPONENT_Z_INCLUDE ${NCBI_COMPONENT_LocalZ_INCLUDE})
  set(NCBI_COMPONENT_Z_NCBILIB ${NCBI_COMPONENT_LocalZ_NCBILIB})
endif()

#############################################################################
# BZ2
if(BZIP2_FOUND AND NOT USE_LOCAL_BZLIB)
  set(NCBI_COMPONENT_BZ2_FOUND YES)
  set(NCBI_COMPONENT_BZ2_INCLUDE ${BZIP2_INCLUDE_DIR})
  set(NCBI_COMPONENT_BZ2_LIBS ${BZIP2_LIBRARIES})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} BZ2")
else()
  set(NCBI_COMPONENT_BZ2_FOUND ${NCBI_COMPONENT_LocalBZ2_FOUND})
  set(NCBI_COMPONENT_BZ2_INCLUDE ${NCBI_COMPONENT_LocalBZ2_INCLUDE})
  set(NCBI_COMPONENT_BZ2_NCBILIB ${NCBI_COMPONENT_LocalBZ2_NCBILIB})
endif()

#############################################################################
#LZO
if (LZO_FOUND)
  set(NCBI_COMPONENT_LZO_FOUND YES)
  set(NCBI_COMPONENT_LZO_INCLUDE ${LZO_INCLUDE_DIR})
  set(NCBI_COMPONENT_LZO_LIBS ${LZO_LIBRARIES})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} LZO")
else()
  set(NCBI_COMPONENT_LZO_FOUND YES)
endif()

#############################################################################
#BerkeleyDB
if(BERKELEYDB_FOUND)
  set(NCBI_COMPONENT_BerkeleyDB_FOUND YES)
  set(NCBI_COMPONENT_BerkeleyDB_INCLUDE ${BerkeleyDB_INCLUDE_DIR})
  set(NCBI_COMPONENT_BerkeleyDB_LIBS ${BerkeleyDB_LIBRARIES})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} BerkeleyDB")
else()
  set(NCBI_COMPONENT_BerkeleyDB_FOUND NO)
endif()

#############################################################################
#LMDB
if(LMDB_FOUND)
  set(NCBI_COMPONENT_LMDB_FOUND YES)
  set(NCBI_COMPONENT_LMDB_INCLUDE ${LMDB_INCLUDE})
  set(NCBI_COMPONENT_LMDB_LIBS ${LMDB_LIBS})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} LMDB")
else()
  set(NCBI_COMPONENT_LMDB_FOUND ${NCBI_COMPONENT_LocalLMDB_FOUND})
  set(NCBI_COMPONENT_LMDB_INCLUDE ${NCBI_COMPONENT_LocalLMDB_INCLUDE})
  set(NCBI_COMPONENT_LMDB_NCBILIB ${NCBI_COMPONENT_LocalLMDB_NCBILIB})
endif()

#############################################################################
# JPEG
if(HAVE_LIBJPEG)
  set(NCBI_COMPONENT_JPEG_FOUND YES)
  set(NCBI_COMPONENT_JPEG_LIBS -ljpeg)
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} JPEG")
else()
  set(NCBI_COMPONENT_JPEG_FOUND NO)
endif()

#############################################################################
# PNG
if(HAVE_LIBPNG)
  set(NCBI_COMPONENT_PNG_FOUND YES)
  set(NCBI_COMPONENT_PNG_LIBS -lpng)
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} PNG")
else()
  set(NCBI_COMPONENT_PNG_FOUND NO)
endif()

#############################################################################
# GIF
set(NCBI_COMPONENT_GIF_FOUND YES)
  set(NCBI_COMPONENT_GIF_LIBS -lgif)
set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} GIF")

#############################################################################
# TIFF
if(HAVE_LIBTIFF)
  set(NCBI_COMPONENT_TIFF_FOUND YES)
  set(NCBI_COMPONENT_TIFF_LIBS -ltiff)
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} TIFF")
else()
  set(NCBI_COMPONENT_TIFF_FOUND NO)
endif()

#############################################################################
# XML
if(LIBXML2_FOUND)
  set(NCBI_COMPONENT_XML_FOUND YES)
  set(NCBI_COMPONENT_XML_INCLUDE ${LIBXML_INCLUDE})
  set(NCBI_COMPONENT_XML_LIBS ${LIBXML_LIBS})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} XML")
else()
  set(NCBI_COMPONENT_XML_FOUND NO)
endif()

#############################################################################
# XSLT
# EXSLT
if(LIBXSLT_FOUND)
  set(NCBI_COMPONENT_XSLT_FOUND YES)
  set(NCBI_COMPONENT_XSLT_INCLUDE ${LIBXSLT_INCLUDE})
  set(NCBI_COMPONENT_XSLT_LIBS ${LIBXSLT_LIBS})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} XSLT")

  set(NCBI_COMPONENT_EXSLT_FOUND YES)
  set(NCBI_COMPONENT_EXSLT_INCLUDE ${LIBEXSLT_INCLUDE})
  set(NCBI_COMPONENT_EXSLT_LIBS ${LIBEXSLT_LIBS})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} EXSLT")
else()
  set(NCBI_COMPONENT_XSLT_FOUND NO)
  set(NCBI_COMPONENT_EXSLT_FOUND NO)
endif()

#############################################################################
# XLSXWRITER
if (LIBXLSXWRITER_FOUND)
  set(NCBI_COMPONENT_XLSXWRITER_FOUND YES)
  set(NCBI_COMPONENT_XLSXWRITER_INCLUDE ${LIBXLSXWRITER_INCLUDE})
  set(NCBI_COMPONENT_XLSXWRITER_LIBS ${LIBXLSXWRITER_LIBS})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} XLSXWRITER")
endif()

#############################################################################
#SQLITE3
if(SQLITE3_FOUND)
  set(NCBI_COMPONENT_SQLITE3_FOUND YES)
  set(NCBI_COMPONENT_SQLITE3_INCLUDE ${SQLITE3_INCLUDE})
  set(NCBI_COMPONENT_SQLITE3_LIBS ${SQLITE3_LIBS})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} SQLITE3")
else()
  set(NCBI_COMPONENT_SQLITE3_FOUND NO)
endif()

#############################################################################
#LAPACK
if(LAPACK_FOUND)
  set(NCBI_COMPONENT_LAPACK_FOUND YES)
  set(NCBI_COMPONENT_LAPACK_INCLUDE ${LAPACK_INCLUDE})
  set(NCBI_COMPONENT_LAPACK_LIBS ${LAPACK_LIBS})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} LAPACK")
else()
  set(NCBI_COMPONENT_LAPACK_FOUND NO)
endif()

#############################################################################
# Sybase
if(SYBASE_FOUND)
  set(NCBI_COMPONENT_Sybase_FOUND YES)
  set(NCBI_COMPONENT_Sybase_INCLUDE ${SYBASE_INCLUDE})
  set(NCBI_COMPONENT_Sybase_LIBS    ${SYBASE_LIBS})
  set(NCBI_COMPONENT_Sybase_DEFINES SYB_LP64)
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} Sybase")
else()
  set(NCBI_COMPONENT_Sybase_FOUND NO)
endif()

#############################################################################
# MySQL
if(MYSQL_FOUND)
  set(NCBI_COMPONENT_MySQL_FOUND YES)
  set(NCBI_COMPONENT_MySQL_INCLUDE ${MYSQL_INCLUDE})
  set(NCBI_COMPONENT_MySQL_LIBS    ${MYSQL_LIBS})
else()
  set(NCBI_COMPONENT_MySQL_FOUND NO)
endif()

#############################################################################
# ODBC
if(ODBC_FOUND)
  set(NCBI_COMPONENT_ODBC_FOUND YES)
  set(NCBI_COMPONENT_ODBC_INCLUDE ${ODBC_INCLUDE})
  set(NCBI_COMPONENT_ODBC_LIBS    ${ODBC_LIBS})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} ODBC")
else()
  set(NCBI_COMPONENT_ODBC_FOUND NO)
endif()

#############################################################################
# VDB
if(VDB_FOUND)
  set(NCBI_COMPONENT_VDB_FOUND YES)
  set(NCBI_COMPONENT_VDB_INCLUDE ${VDB_INCLUDE})
  set(NCBI_COMPONENT_VDB_LIBS    ${VDB_LIBS})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} VDB")
else()
  set(NCBI_COMPONENT_VDB_FOUND NO)
endif()

#############################################################################
# SAMTOOLS
if(SAMTOOLS_FOUND)
  set(NCBI_COMPONENT_SAMTOOLS_FOUND YES)
  set(NCBI_COMPONENT_SAMTOOLS_INCLUDE ${SAMTOOLS_INCLUDE})
  set(NCBI_COMPONENT_SAMTOOLS_LIBS    ${SAMTOOLS_LIBS})
#  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} SAMTOOLS")
else()
  set(NCBI_COMPONENT_SAMTOOLS_FOUND NO)
endif()

#############################################################################
# PYTHON
set(NCBI_COMPONENT_PYTHON_FOUND NO)

##############################################################################
# GRPC/PROTOBUF

set(NCBI_ThirdParty_GRPC "/netopt/ncbi_tools64/grpc-1.21.1-ncbi1/${NCBI_COMPILER}${NCBI_COMPILER_VERSION}-${NCBI_BUILD_TYPE}")

if(OFF)
set(Protobuf_SRC_ROOT_FOLDER ${NCBI_ThirdParty_GRPC})
set(Protobuf_USE_STATIC_LIBS YES)
find_package(Protobuf)
message("================================================================")
message("Protobuf_FOUND = ${Protobuf_FOUND}")
message("Protobuf_INCLUDE_DIRS = ${Protobuf_INCLUDE_DIRS}")
message("Protobuf_LIBRARIES = ${Protobuf_LIBRARIES}")
message("Protobuf_PROTOC_LIBRARIES = ${Protobuf_PROTOC_LIBRARIES}")
message("================================================================")

else()

set(NCBI_PROTOC_APP "${NCBI_ThirdParty_GRPC}/bin/protoc")
set(NCBI_GRPC_PLUGIN "${NCBI_ThirdParty_GRPC}/bin/grpc_cpp_plugin")

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  set(_suffix "d.a")
else()
  set(_suffix ".a")
endif()

set(NCBI_ThirdParty_GRPC_LIBS "${NCBI_ThirdParty_GRPC}/lib64/lib")
if (EXISTS "${NCBI_ThirdParty_GRPC}/include" AND EXISTS "${NCBI_ThirdParty_GRPC_LIBS}protobuf${_suffix}")
  set(NCBI_COMPONENT_PROTOBUF_FOUND YES)
  set(NCBI_COMPONENT_PROTOBUF_INCLUDE ${NCBI_ThirdParty_GRPC}/include)
  set(NCBI_COMPONENT_PROTOBUF_LIBS ${NCBI_ThirdParty_GRPC_LIBS}protobuf${_suffix})
  message("PROTOBUF found at ${NCBI_ThirdParty_GRPC}")
else()
  set(NCBI_COMPONENT_PROTOBUF_FOUND NO)
endif()

if(NCBI_COMPONENT_PROTOBUF_FOUND)
  set(NCBI_COMPONENT_GRPC_FOUND YES)
  set(NCBI_COMPONENT_GRPC_INCLUDE ${NCBI_ThirdParty_GRPC}/include)
  set(NCBI_COMPONENT_GRPC_LIBS
    ${NCBI_ThirdParty_GRPC_LIBS}grpc++.a
    ${NCBI_ThirdParty_GRPC_LIBS}grpc.a
    ${NCBI_ThirdParty_GRPC_LIBS}gpr.a
    ${NCBI_COMPONENT_PROTOBUF_LIBS}
    ${NCBI_ThirdParty_GRPC_LIBS}cares.a
    ${NCBI_ThirdParty_GRPC_LIBS}address_sorting.a
    ${NCBI_ThirdParty_GRPC_LIBS}boringssl.a
    ${NCBI_ThirdParty_GRPC_LIBS}boringcrypto.a
  )
endif()
endif()

#############################################################################
# OpenSSL
if (OpenSSL_FOUND)
  set(NCBI_COMPONENT_OpenSSL_FOUND YES)
  set(NCBI_COMPONENT_OpenSSL_INCLUDE ${OpenSSL_INCLUDE})
  set(NCBI_COMPONENT_OpenSSL_LIBS    ${OPENSSL_LIBS})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} OpenSSL")
else()
  set(NCBI_COMPONENT_OpenSSL_FOUND NO)
endif()

#############################################################################
# MSGSL
if(HAVE_MSGSL)
  set(NCBI_COMPONENT_MSGSL_FOUND YES)
  set(NCBI_COMPONENT_MSGSL_INCLUDE ${MSGSL_INCLUDE})
endif()

#############################################################################
# SGE
if (SGE_FOUND)
  set(NCBI_COMPONENT_SGE_FOUND YES)
  set(NCBI_COMPONENT_SGE_INCLUDE ${SGE_INCLUDE})
  set(NCBI_COMPONENT_SGE_LIBS    ${SGE_LIBS})
  set(HAVE_LIBSGE 1)
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} SGE")
endif()

#############################################################################
# MONGOCXX
if (MONGOCXX_FOUND)
  set(NCBI_COMPONENT_MONGOCXX_FOUND YES)
  set(NCBI_COMPONENT_MONGOCXX_INCLUDE ${MONGOCXX_INCLUDE})
  set(NCBI_COMPONENT_MONGOCXX_LIBS    ${MONGOCXX_LIB})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} MONGOCXX")
endif()

#############################################################################
# LEVELDB
if (LEVELDB_FOUND)
  set(NCBI_COMPONENT_LEVELDB_FOUND YES)
#  set(NCBI_COMPONENT_LEVELDB_INCLUDE ${LEVELDB_INCLUDE})
  set(NCBI_COMPONENT_LEVELDB_LIBS    ${LEVELDB_LIBRARIES})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} LEVELDB")
endif()

#############################################################################
# WGMLST
if (WGMLST_FOUND)
  set(NCBI_COMPONENT_WGMLST_FOUND YES)
  set(NCBI_COMPONENT_WGMLST_INCLUDE ${WGMLST_INCLUDE})
  set(NCBI_COMPONENT_WGMLST_LIBS    ${WGMLST_LIB})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} WGMLST")
endif()

#############################################################################
# GLPK
if(GLPK_FOUND)
  set(NCBI_COMPONENT_GLPK_FOUND YES)
  set(NCBI_COMPONENT_GLPK_INCLUDE ${GLPK_INCLUDE})
  set(NCBI_COMPONENT_GLPK_LIBS    ${GLPK_LIBS})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} GLPK")
endif()

#############################################################################
# XALAN
if (XALAN_FOUND)
  set(NCBI_COMPONENT_XALAN_FOUND YES)
  set(NCBI_COMPONENT_XALAN_INCLUDE ${XALAN_INCLUDE})
  set(NCBI_COMPONENT_XALAN_LIBS    ${XALAN_LIBS})
  set(HAVE_XALAN 1)
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} XALAN")
endif()

#############################################################################
# XERCES
if (XERCES_FOUND)
  set(NCBI_COMPONENT_XERCES_FOUND YES)
  set(NCBI_COMPONENT_XERCES_INCLUDE ${XERCES_INCLUDE})
  set(NCBI_COMPONENT_XERCES_LIBS    ${XERCES_LIBS})
  set(HAVE_XERCES 1)
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} XERCES")
endif()

##############################################################################
# FTGL
if (FTGL_FOUND)
  set(NCBI_COMPONENT_FTGL_FOUND   YES)
  set(NCBI_COMPONENT_FTGL_INCLUDE ${FTGL_INCLUDE_DIR})
  set(NCBI_COMPONENT_FTGL_LIBS    ${FTGL_LIBRARIES})
  set(HAVE_LIBFTGL 1)
  list(APPEND NCBI_ALL_COMPONENTS FTGL)
endif()

##############################################################################
# FreeType
if (FREETYPE_FOUND)
  set(NCBI_COMPONENT_FreeType_FOUND   YES)
  set(NCBI_COMPONENT_FreeType_INCLUDE ${FREETYPE_INCLUDE_DIRS})
  set(NCBI_COMPONENT_FreeType_LIBS    ${FREETYPE_LIBRARIES})
  set(HAVE_FREETYPE 1)
  list(APPEND NCBI_ALL_COMPONENTS FreeType)
endif()

#############################################################################
# GLEW
set(NCBI_COMPONENT_GLEW_FOUND   YES)
set(NCBI_COMPONENT_GLEW_INCLUDE ${GLEW_INCLUDE_DIRS})
set(NCBI_COMPONENT_GLEW_LIBS    ${GLEW_LIBRARIES})
set(HAVE_LIBGLEW 1)
if(NCBI_COMPONENT_GLEW_FOUND)
  if(BUILD_SHARED_LIBS)
    set(NCBI_COMPONENT_GLEW_DEFINES GLEW_MX)
  else()
    set(NCBI_COMPONENT_GLEW_DEFINES GLEW_MX GLEW_STATIC)
  endif()
endif()
list(APPEND NCBI_ALL_COMPONENTS GLEW)

##############################################################################
# OpenGL
set(NCBI_COMPONENT_OpenGL_FOUND   YES)
set(NCBI_COMPONENT_OpenGL_INCLUDE ${OPENGL_INCLUDE_DIRS})
set(NCBI_COMPONENT_OpenGL_LIBS    ${OPENGL_LIBRARIES})
set(HAVE_OPENGL 1)
list(APPEND NCBI_ALL_COMPONENTS OpenGL)

##############################################################################
# OSMesa
set(NCBI_COMPONENT_OSMesa_FOUND   YES)
set(NCBI_COMPONENT_OSMesa_INCLUDE ${OSMesa_INCLUDE_DIRS})
set(NCBI_COMPONENT_OSMesa_LIBS    ${OSMesa_LIBRARIES})
set(HAVE_LIBOSMESA 1)
list(APPEND NCBI_ALL_COMPONENTS OSMesa)

##############################################################################
# wxWidgets
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

##############################################################################
# GCRYPT
#set(GCRYPT_ROOT_DIR /netopt/ncbi_tools64/libgcrypt-1.4.3
#include(${NCBI_TREE_CMAKECFG}/FindGCrypt.cmake)
set(NCBI_ThirdParty_GCRYPT "/netopt/ncbi_tools64/libgcrypt-1.4.3")
set(NCBI_COMPONENT_GCRYPT_FOUND YES)
set(NCBI_COMPONENT_GCRYPT_INCLUDE ${NCBI_ThirdParty_GCRYPT}/include /netopt/ncbi_tools64/libgpg-error-1.6/include)
set(HAVE_LIBGCRYPT 1)
set(NCBI_COMPONENT_GCRYPT_LIBS
    ${NCBI_ThirdParty_GCRYPT}/lib64/libgcrypt.a
    /netopt/ncbi_tools64/libgpg-error-1.6/lib64/libgpg-error.a
)

##############################################################################
##############################################################################
set(NCBI_ThirdParty_UV           ${NCBI_TOOLS_ROOT}/libuv-1.25.0)
set(NCBI_ThirdParty_NGHTTP2      ${NCBI_TOOLS_ROOT}/nghttp2-1.29.0)

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
    elseif (EXISTS ${_root}/${CMAKE_BUILD_TYPE}${NCBI_PlatformBits}/include)
        set(NCBI_COMPONENT_${_name}_INCLUDE ${_root}/${CMAKE_BUILD_TYPE}${NCBI_PlatformBits}/include PARENT_SCOPE)
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
    set(_subdirs ${NCBI_COMPILER}${NCBI_COMPILER_VERSION}-${NCBI_BUILD_TYPE}/lib ${NCBI_BUILD_TYPE}/lib ${CMAKE_BUILD_TYPE}${NCBI_PlatformBits}/lib lib64 lib)
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
# UV
NCBI_define_component(UV uv)

#############################################################################
# NGHTTP2
NCBI_define_component(NGHTTP2 nghttp2)

