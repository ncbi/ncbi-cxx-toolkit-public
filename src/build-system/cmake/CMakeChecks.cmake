############################################################################
#
# Basic Setup
#


set(top_src_dir     ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(abs_top_src_dir ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(build_root      ${CMAKE_BINARY_DIR})
set(builddir        ${CMAKE_BINARY_DIR})
set(includedir0     ${top_src_dir}/include)
set(includedir      ${includedir0})
set(incdir          ${build_root}/inc)
set(incinternal     ${includedir0}/internal)

set(NCBI_TOOLS_ROOT $ENV{NCBI})

# pass these back for ccache to pick up
set(ENV{CCACHE_UMASK} 002)
set(ENV{CCACHE_BASEDIR} ${top_src_dir})

if (NOT buildconf)
  set(buildconf "${CMAKE_BUILD_TYPE}MT64")
  set(buildconf0 ${CMAKE_BUILD_TYPE})
endif (NOT buildconf)

get_filename_component(top_src_dir "${top_src_dir}" REALPATH)
get_filename_component(abs_top_src_dir "${abs_top_src_dir}" REALPATH)
get_filename_component(build_root "${build_root}" REALPATH)
get_filename_component(includedir "${includedir}" REALPATH)

get_filename_component(EXECUTABLE_OUTPUT_PATH "${build_root}/../bin" REALPATH)
get_filename_component(LIBRARY_OUTPUT_PATH "${build_root}/../lib" REALPATH)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/build-system/cmake/")

# Establishing compiler definitions
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.compiler.cmake)

# Establishing sane RPATH definitions
# use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, use the install RPATH already
SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

SET(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib")

#this add RUNPATH to binaries (RPATH is already there anyway), which makes it more like binaries built by C++ Toolkit
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--enable-new-dtags")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH true)

############################################################################
#
# Testing
enable_testing()


#
# Basic checks
#
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.basic-checks.cmake)

#
# Framework for dealing with external libraries
#
include(${top_src_dir}/src/build-system/cmake/FindExternalLibrary.cmake)

############################################################################
#
# PCRE additions
#
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.pcre.cmake)

############################################################################
#
# Compression libraries
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.compress.cmake)



############################################################################
#
# OS-specific settings
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.os.cmake)


#################################
# Some platform-specific system libs that can be linked eventually
set(THREAD_LIBS   ${CMAKE_THREAD_LIBS_INIT})
set(KSTAT_LIBS    )
set(RPCSVC_LIBS   )
set(DEMANGLE_LIBS )
set(ICONV_LIBS    )

find_library(UUID_LIBS NAMES uuid)
find_library(CRYPT_LIB NAMES crypt)
find_library(MATH_LIBS NAMES m)
find_library(GCRYPT_LIB NAMES gcrypt)

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
set(NCBI_CRYPT ncbi_crypt)


# non-public (X)CONNECT extensions
set(CONNEXT connext)
set(XCONNEXT xconnext)

# NCBI C++ API for BerkeleyDB
set(BDB_LIB bdb)
set(BDB_CACHE_LIB ncbi_xcache_bdb)

# Possibly absent DBAPI drivers (depending on whether the relevant
# 3rd-party libraries are present, and whether DBAPI was disabled altogether)
set(DBAPI_DRIVER  dbapi_driver)
set(DBAPI_CTLIB   ncbi_xdbapi_ctlib)
set(DBAPI_DBLIB   ncbi_xdbapi_dblib)
set(DBAPI_MYSQL   ncbi_xdbapi_mysql)
set(DBAPI_ODBC                     )

############################################################################
#
# OS-specific settings
find_library(GMP_LIB LIBS gmp HINTS "/netopt/ncbi_tools64/gmp-6.0.0a/lib64/")
find_library(IDN_LIB LIBS idn HINTS "/lib64")
find_library(NETTLE_LIB LIBS nettle HINTS "/netopt/ncbi_tools64/nettle-3.1.1/lib64")
find_library(HOGWEED_LIB LIBS hogweed HINTS "/netopt/ncbi_tools64/nettle-3.1.1/lib64")
find_external_library(GnuTLS INCLUDES gnutls/gnutls.h LIBS gnutls HINTS "${NCBI_TOOLS_ROOT}/gnutls-3.4.0/" EXTRALIBS ${Z_LIBS} ${IDN_LIB} ${RT_LIBS} ${HOGWEED_LIB} ${NETTLE_LIB} ${GMP_LIB})
#message(FATAL_ERROR "RES: " ${GNUTLS_LIBS})

############################################################################
#
# Kerberos 5 (via GSSAPI)
# FIXME: replace with native CMake check
#
set(KRB5_INCLUDE)
set(KRB5_LIBS   -lgssapi_krb5 -lkrb5 -lk5crypto -lcom_err)


############################################################################
#
# Sybase stuff
#find_package(Sybase)
find_external_library(Sybase DYNAMIC_ONLY INCLUDES sybdb.h LIBS sybblk_r64 sybdb64 sybct_r64 sybcs_r64 sybtcl_r64 sybcomn_r64 sybintl_r64 sybunic64 HINTS "/opt/sybase/clients/15.7-64bit/OCS-15_0/")
set(SYBASE_DBLIBS "${SYBASE_LIBS}")

############################################################################
#
# FreeTDS
# FIXME: do we need these anymore?
#
set(ftds64   ftds64)
set(FTDS64_CTLIB_LIBS  ${ICONV_LIBS} ${KRB5_LIBS})
set(FTDS64_CTLIB_LIB   ct_ftds64 tds_ftds64)
set(FTDS64_CTLIB_INCLUDE ${includedir}/dbapi/driver/ftds64/freetds)
set(FTDS64_LIBS        ${FTDS64_CTLIB_LIBS})
set(FTDS64_LIB        ${FTDS64_CTLIB_LIB})
set(FTDS64_INCLUDE    ${FTDS64_CTLIB_INCLUDE})

set(ftds95   ftds95)
set(FTDS95_CTLIB_LIBS  ${ICONV_LIBS} ${KRB5_LIBS})
set(FTDS95_CTLIB_LIB   ct_ftds95 tds_ftds95)
set(FTDS95_CTLIB_INCLUDE ${includedir}/dbapi/driver/ftds95/freetds)
set(FTDS95_LIBS        ${FTDS95_CTLIB_LIBS})
set(FTDS95_LIB        ${FTDS95_CTLIB_LIB})
set(FTDS95_INCLUDE    ${FTDS95_CTLIB_INCLUDE})

set(FTDS_LIBS     ${FTDS64_LIBS})
set(FTDS_LIB      ${FTDS64_LIB})
set(FTDS_INCLUDE  ${FTDS64_INCLUDE})


############################################################################
#
# MySQL: headers and libs
# FIXME: replace with native CMake check
#
find_package(Mysql)
set(MYSQL_INCLUDE ${MYSQL_INCLUDE_DIR})
set(MYSQL_LIBS ${MYSQL_LIBRARIES})


############################################################################
#
# BerkeleyDB
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.BerkeleyDB.cmake)

# ODBC
# FIXME: replace with native CMake check
#
set(ODBC_INCLUDE  ${includedir}/dbapi/driver/odbc/unix_odbc 
                  ${includedir0}/dbapi/driver/odbc/unix_odbc)
set(ODBC_LIBS)

# Python
find_external_library(Python INCLUDES Python.h LIBS python2.7 INCLUDE_HINTS "/opt/python-2.7env/include/python2.7/" LIBS_HINTS "/opt/python-2.7env/lib/")

############################################################################
#
# Boost settings
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.boost.cmake)

############################################################################
#
# NCBI C Toolkit:  headers and libs
string(REGEX MATCH "DNCBI_INT8_GI|NCBI_STRICT_GI" INT8GI_FOUND "${CMAKE_CXX_FLAGS}")
if (NOT "${INT8GI_FOUND}" STREQUAL "")
    if (EXISTS "${NCBI_TOOLS_ROOT}/ncbi/ncbi.gi64/")
        set(NCBI_CTOOLKIT_PATH "${NCBI_TOOLS_ROOT}/ncbi/ncbi.gi64/")
    elseif (EXISTS "${NCBI_TOOLS_ROOT}/ncbi.gi64")
        set(NCBI_CTOOLKIT_PATH "${NCBI_TOOLS_ROOT}/ncbi.gi64/")
    else ()
        set(NCBI_CTOOLKIT_PATH "${NCBI_TOOLS_ROOT}/ncbi/")
    endif ()
else ()
    set(NCBI_CTOOLKIT_PATH "${NCBI_TOOLS_ROOT}/ncbi/")
endif ()

if (EXISTS "${NCBI_CTOOLKIT_PATH}/include64" AND EXISTS "${NCBI_CTOOLKIT_PATH}/lib64")
    set(NCBI_C_INCLUDE  "${NCBI_CTOOLKIT_PATH}/include64")
    set(NCBI_C_LIBPATH  "-L${NCBI_CTOOLKIT_PATH}/lib64")
    set(NCBI_C_ncbi     "${NCBI_C_LIBPATH} -lncbi")
    if (APPLE)
        set(NCBI_C_ncbi ${NCBI_C_ncbi} -Wl,-framework,ApplicationServices)
    endif ()
    set(HAVE_NCBI_C true)
else ()
    set(HAVE_NCBI_C false)
endif ()

############################################################################
#
# OpenGL: headers and libs (including core X dependencies) for code
# not using other toolkits.  (The wxWidgets variables already include
# these as appropriate.)

find_external_library(OpenGL INCLUDES GL/gl.h LIBS GLU GL Xmu Xt Xext SM ICE X11 HINTS "${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2")

if (${OPENGL_FOUND})
  find_external_library(OSMESA INCLUDES GL/osmesa.h LIBS OSMesa HINTS "${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2" EXTRALIBS ${OPENGL_LIBS})
  #set(OSMESA_STATIC_LIBS  "-L${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2/lib64 -Wl,-rpath,/opt/ncbi/64/Mesa-7.0.2-ncbi2/lib64:${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2/lib64 -lOSMesa-static -lGLU-static -lGL-static -lXmu -lXt -lXext -lSM -lICE -lX11")
else()
  set(OSMESA_FOUND false)
endif()

find_external_library(GLEW INCLUDES GL/glew.h LIBS GLEW HINTS "${NCBI_TOOLS_ROOT}/glew-1.5.8/GCC401-Debug64")

############################################################################
#
# wxWidgets
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.wxwidgets.cmake)

# Fast-CGI
if (APPLE)
  find_external_library(FastCGI INCLUDES fastcgi.h LIBS fcgi HINTS "${NCBI_TOOLS_ROOT}/fcgi-2.4.0")
else ()
  if ("${BUILD_SHARED_LIBS}" STREQUAL "OFF")
    find_external_library(FastCGI INCLUDES fastcgi.h LIBS fcgi INCLUDE_HINTS "${NCBI_TOOLS_ROOT}/fcgi-2.4.0/include" LIBS_HINTS "${NCBI_TOOLS_ROOT}/fcgi-2.4.0/lib" EXTRALIBS ${NETWORK_LIBS})
  else ()
    find_external_library(FastCGI INCLUDES fastcgi.h LIBS fcgi INCLUDE_HINTS "${NCBI_TOOLS_ROOT}/fcgi-2.4.0/include" LIBS_HINTS "${NCBI_TOOLS_ROOT}/fcgi-2.4.0/shlib" EXTRALIBS ${NETWORK_LIBS})
  endif()
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
find_external_library(ICU INCLUDES unicode/ucnv.h LIBS icui18n icuuc icudata HINTS "${NCBI_TOOLS_ROOT}/icu-49.1.1")

find_external_library(libxml DYNAMIC_ONLY INCLUDES libxml/xmlwriter.h LIBS xml2 INCLUDE_HINTS "${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/include/libxml2/" LIBS_HINTS "${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/lib")
set(LIBXML_INCLUDE ${LIBXML_INCLUDE} "${LIBXML_INCLUDE}/../")

find_external_library(libexslt DYNAMIC_ONLY INCLUDES libexslt/exslt.h LIBS exslt HINTS "${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/" EXTRALIBS ${LIBXML_LIBS} ${GCRYPT_LIB})
if (${LIBEXSLT_FOUND})
   set(LIBEXSLT_INCLUDE ${LIBEXSLT_INCLUDE} "${LIBEXSLT_INCLUDE}/../")
endif ()

find_external_library(libxslt DYNAMIC_ONLY INCLUDES libxslt/xslt.h LIBS xslt HINTS "${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/" EXTRALIBS ${LIBEXSLT_LIBS})
if (${LIBXSLT_FOUND})
  set(LIBXSLT_INCLUDE ${LIBXSLT_INCLUDES} "${LIBXSLT_INCLUDES}/../")
  set(LIBXSLT_LIBS ${LIBEXSLT_LIBS} ${LIBXSLT_LIBS})
endif ()

if (EXISTS ${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/lib/libxslt-static.a)
  set(LIBXSLT_STATIC_LIBS ${LIBEXSLT_STATIC_LIBS} ${LIBXSLT_MAIN_STATIC_LIBS})
endif (EXISTS ${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/lib/libxslt-static.a)

find_external_library(xerces INCLUDES xercesc/dom/DOM.hpp LIBS xerces-c HINTS "${NCBI_TOOLS_ROOT}/xerces-3.1.1/GCC442-DebugMT64")
#set(XERCES_STATIC_LIBS -L${NCBI_TOOLS_ROOT}/xerces-3.1.1/GCC442-DebugMT64/lib -lxerces-c-static -lcurl )

find_external_library(xalan INCLUDES xalanc/XalanTransformer/XalanTransformer.hpp LIBS xalan-c xalanMsg HINTS "${NCBI_TOOLS_ROOT}/xalan-1.11~r1302529/GCC442-DebugMT64")

# Sun Grid Engine (libdrmaa):
# libpath - /netmnt/uge/lib/lx-amd64/
find_external_library(SGE INCLUDES drmaa.h LIBS drmaa INCLUDE_HINTS "/netmnt/uge/include" LIBS_HINTS "/netmnt/uge/lib/lx-amd64/")

# muParser
find_external_library(muparser INCLUDES muParser.h LIBS muparser INCLUDE_HINTS "${NCBI_TOOLS_ROOT}/muParser-1.30/include" LIBS_HINTS "${NCBI_TOOLS_ROOT}/muParser-1.30/GCC-Debug64/lib/")

# HDF5
find_external_library(hdf5 INCLUDES hdf5.h LIBS hdf5 HINTS "${NCBI_TOOLS_ROOT}/hdf5-1.8.3/GCC401-Debug64/")

############################################################################
#
# Various image-format libraries
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.sqlite3.cmake)

############################################################################
#
# Various image-format libraries
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.image.cmake)

# libmagic (file-format identification)
find_library(MAGIC_LIBS magic)

# libcurl (URL retrieval)
find_library(CURL_LIBS curl)

# libmimetic (MIME handling)
find_external_library(mimetic INCLUDES mimetic/mimetic.h LIBS mimetic HINTS "${NCBI_TOOLS_ROOT}/mimetic-0.9.7-ncbi1/")

# libgSOAP++
find_external_library(gsoap INCLUDES stdsoap2.h LIBS gsoapssl++ INCLUDE_HINTS "${NCBI_TOOLS_ROOT}/gsoap-2.8.15/include" LIBS_HINTS "${NCBI_TOOLS_ROOT}/gsoap-2.8.15/GCC442-DebugMT64/lib/" EXTRALIBS ${Z_LIBS})

set(GSOAP_SOAPCPP2 ${NCBI_TOOLS_ROOT}/gsoap-2.8.15/GCC442-DebugMT64/bin/soapcpp2)
set(GSOAP_WSDL2H   ${NCBI_TOOLS_ROOT}/gsoap-2.8.15/GCC442-DebugMT64/bin/wsdl2h)

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
find_external_library(VDB INCLUDES sra/sradb.h LIBS ncbi-vdb INCLUDE_HINTS "/opt/ncbi/64/trace_software/vdb/vdb-versions/2.8.0/interfaces" LIBS_HINTS "/opt/ncbi/64/trace_software/vdb/vdb-versions/2.8.0/linux/release/x86_64/lib")
if (${VDB_FOUND})
    set(VDB_INCLUDE "${VDB_INCLUDE}" "${VDB_INCLUDE}/os/linux" "${VDB_INCLUDE}/os/unix" "${VDB_INCLUDE}/cc/gcc/x86_64" "${VDB_INCLUDE}/cc/gcc")
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
find_external_library(glpk INCLUDES glpk.h LIBS glpk HINTS "/usr/local/glpk/4.45")
#message(FATAL_ERROR "exit")

find_external_library(samtools INCLUDES bam.h LIBS bam HINTS "${NCBI_TOOLS_ROOT}/samtools")

#LAPACK
set(LAPACK_LIBS "-llapack -lblas")

#LMBD
set(LMDB_INCLUDE "/netopt/ncbi_tools64/lmdb-0.9.18/include")
set(LMDB_LIBS -L/netopt/ncbi_tools64/lmdb-0.9.18/lib64 -llmdb)

find_external_library(libxlsxwriter INCLUDES xlsxwriter.h LIBS xlsxwriter HINTS "${NCBI_TOOLS_ROOT}/libxlsxwriter-0.6.9" EXTRALIBS ${Z_LIBS})

##############################################################################
#
# NCBI-isms
# FIXME: these should be tested not hard-coded
set (NCBI_DATATOOL ${NCBI_TOOLS_ROOT}/bin/datatool)
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.mongodb.cmake)


#
# Final tasks
#

if (UNIX)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build-system/config.cmake.h.in ${build_root}/inc/ncbiconf_unix.h)
endif(UNIX)

if (WIN32)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build-system/config.cmake.h.in ${build_root}/inc/ncbiconf_msvc.h)
endif (WIN32)

if (APPLE AND NOT UNIX) #XXX 
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build-system/config.cmake.h.in ${build_root}/inc/ncbiconf_xcode.h)
endif (APPLE AND NOT UNIX)

#
# write ncbicfg.c.in
#
# FIXME:
# We need to set these variables to get them into the cfg file:
#  - c_ncbi_runpath
#  - SYBASE_LCL_PATH
#  - SYBASE_PATH
#  - FEATURES
set(c_ncbi_runpath "\$ORIGIN/../lib")
set(SYBASE_LCL_PATH ${SYBASE_LIBRARIES})
set(SYBASE_PATH "")
set(FEATURES "")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/corelib/ncbicfg.c.in ${CMAKE_BINARY_DIR}/corelib/ncbicfg.c)

ENABLE_TESTING()
#include_directories(${CPP_INCLUDE_DIR} ${build_root}/inc)
include_directories(${incdir} ${includedir0} ${incinternal})

#
# Dump our final diagnostics
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.final-message.cmake)

