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

# when building, don't use the install RPATH already
# (but later on when installing)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE) 

SET(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH false)


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

find_library(UUID_LIBS      uuid)
find_library(CRYPT_LIBS     crypt)
find_library(MATH_LIBS      m)

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
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.openssl.cmake)


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
# FIXME: replace with native CMake check
#
find_package(Sybase)

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

############################################################################
#
# PYTHON: headers and libs (default + specific versions)
# FIXME: replace with native CMake check
#
set(PYTHON_INCLUDE  /opt/python-2.7env/include/python2.7)
set(PYTHON_LIBS -L/opt/python-2.7/lib/ -Wl,-rpath,/opt/python-2.7/lib/ -lpython2.7)
#set(PYTHON_LIBS     -L/opt/python-2.5/lib -L/opt/python-2.5/lib/python2.5/config -Wl,-rpath,/opt/python-2.5/lib:/opt/python-2.5/lib/python2.5/config -lpython2.5 -lpthread -ldl  -lutil -lm)
set(PYTHON23_INCLUDE)
set(PYTHON23_LIBS)
set(PYTHON24_INCLUDE)
set(PYTHON24_LIBS)
set(PYTHON25_INCLUDE  /opt/python-2.5/include/python2.5)
set(PYTHON25_LIBS     -L/opt/python-2.5/lib -L/opt/python-2.5/lib/python2.5/config -Wl,-rpath,/opt/python-2.5/lib:/opt/python-2.5/lib/python2.5/config -lpython2.5 -lpthread -ldl  -lutil -lm)

############################################################################
#
# Boost settings
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.boost.cmake)

############################################################################
#
# NCBI C Toolkit:  headers and libs
# FIXME: use a CMake check for this instead
if (EXISTS "${NCBI_TOOLS_ROOT}/include64" AND EXISTS "${NCBI_TOOLS_ROOT}/lib64") 
    set(NCBI_C_INCLUDE  ${NCBI_TOOLS_ROOT}/include64)
    set(NCBI_C_LIBPATH  "-L${NCBI_TOOLS_ROOT}/lib64")
    set(NCBI_C_ncbi     ${NCBI_C_LIBPATH} -lncbi)
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

find_external_library(OpenGL INCLUDES GL/gl.h LIBS GLU HINTS "${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2" EXTRAFLAGS -lGL -lXmu -lXt -lXext  -lSM -lICE -lX11)
#set(OPENGL_STATIC_LIBS "-L${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2/lib64 -Wl,-rpath,/opt/ncbi/64/Mesa-7.0.2-ncbi2/lib64:${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2/lib64 -lGLU-static -lGL-static -lXmu -lXt -lXext -lSM -lICE -lX11")
if (NOT OPENGL_FOUND)
  #not sure that it works
  find_package(OpenGL)
  set(OPENGL_LIBS "${OPENGL_LIBRARIES}")
endif()

if (OPENGL_FOUND)
  find_external_library(OSMESA INCLUDES GL/osmesa.h LIBS OSMesa HINTS "${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2" EXTRAFLAGS ${OPENGL_LIBS})
  #set(OSMESA_STATIC_LIBS  "-L${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2/lib64 -Wl,-rpath,/opt/ncbi/64/Mesa-7.0.2-ncbi2/lib64:${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2/lib64 -lOSMesa-static -lGLU-static -lGL-static -lXmu -lXt -lXext -lSM -lICE -lX11")
else()
  set(OSMESA_FOUND false)
endif()

find_external_library(GLEW INCLUDES GL/glew.h LIBS GLEW HINTS "${NCBI_TOOLS_ROOT}/glew-1.5.8/GCC401-Debug64")
#set(GLEW_STATIC_LIBS   ${NCBI_TOOLS_ROOT}/glew-1.5.8/GCC401-Debug64/lib/libGLEW-static.a)


############################################################################
#
# wxWidgets
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.wxwidgets.cmake)

# Fast-CGI
if (APPLE)
  find_external_library(FastCGI INCLUDES fastcgi.h LIBS fcgi HINTS "${NCBI_TOOLS_ROOT}/fcgi-2.4.0")
else ()
  find_external_library(FastCGI INCLUDES fastcgi.h LIBS fcgi INCLUDE_HINTS "${NCBI_TOOLS_ROOT}/fcgi-2.4.0/include" LIBS_HINTS "${NCBI_TOOLS_ROOT}/fcgi-2.4.0/shlib" EXTRAFLAGS -lnsl)
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
find_external_library(ICU INCLUDES unicode/ucnv.h LIBS icui18n HINTS "${NCBI_TOOLS_ROOT}/icu-49.1.1" EXTRAFLAGS -licuuc -licudata)
#set(ICU_CONFIG      ${NCBI_TOOLS_ROOT}/icu-49.1.1/GCC442-DebugMT/bin/icu-config)
#set(ICU_STATIC_LIBS -L${NCBI_TOOLS_ROOT}/icu-49.1.1/GCC442-DebugMT/lib  -lsicui18n -lsicuuc -lsicudata )

# XML/XSL support:
set(EXPAT_INCLUDE       )
set(EXPAT_LIBS         -L${NCBI_TOOLS_ROOT}/expat-1.95.8/lib64 -Wl,-rpath,/opt/ncbi/64/expat-1.95.8/lib64:${NCBI_TOOLS_ROOT}/expat-1.95.8/lib64 -lexpat )
set(EXPAT_STATIC_LIBS  -L${NCBI_TOOLS_ROOT}/expat-1.95.8/lib64 -Wl,-rpath,/opt/ncbi/64/expat-1.95.8/lib64:${NCBI_TOOLS_ROOT}/expat-1.95.8/lib64 -lexpat )
set(SABLOT_INCLUDE      ${NCBI_TOOLS_ROOT}/Sablot-1.0.2/include)
set(SABLOT_LIBS        -L${NCBI_TOOLS_ROOT}/Sablot-1.0.2/lib64 -Wl,-rpath,/opt/ncbi/64/Sablot-1.0.2/lib64:${NCBI_TOOLS_ROOT}/Sablot-1.0.2/lib64 -lsablot -L${NCBI_TOOLS_ROOT}/expat-1.95.8/lib64 -Wl,-rpath,/opt/ncbi/64/expat-1.95.8/lib64:${NCBI_TOOLS_ROOT}/expat-1.95.8/lib64 -lexpat )
set(SABLOT_STATIC_LIBS -L${NCBI_TOOLS_ROOT}/Sablot-1.0.2/lib64 -Wl,-rpath,/opt/ncbi/64/Sablot-1.0.2/lib64:${NCBI_TOOLS_ROOT}/Sablot-1.0.2/lib64 -lsablot -L${NCBI_TOOLS_ROOT}/expat-1.95.8/lib64 -Wl,-rpath,/opt/ncbi/64/expat-1.95.8/lib64:${NCBI_TOOLS_ROOT}/expat-1.95.8/lib64 -lexpat )

find_external_library(libxml INCLUDES libxml/xmlwriter.h LIBS xml2 INCLUDE_HINTS "${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/include/libxml2/" LIBS_HINTS "${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/lib")
set(LIBXML_INCLUDE     ${LIBXML_INCLUDE} "${LIBXML_INCLUDE}/../")

#if (EXISTS ${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/lib/libxml2-static.a)
#  set(LIBXML_STATIC_LIBS -L${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/lib -lxml2-static)
#endif(EXISTS ${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/lib/libxml2-static.a)

find_external_library(libexslt INCLUDES libexslt/exslt.h LIBS exslt HINTS "${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/" EXTRAFLAGS ${LIBXML_LIBS})
set(LIBEXSLT_INCLUDE ${LIBEXSLT_INCLUDE} "${LIBEXSLT_INCLUDE}/../")

find_external_library(libxslt INCLUDES libxslt/xslt.h LIBS xslt HINTS "${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/" EXTRAFLAGS ${LIBEXSLT_LIBS})
set(LIBXSLT_INCLUDE ${LIBXSLT_INCLUDES} "${LIBXSLT_INCLUDES}/../")

if (EXISTS ${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/lib/libxslt-static.a)
  set(LIBXSLT_STATIC_LIBS ${LIBEXSLT_STATIC_LIBS} ${LIBXSLT_MAIN_STATIC_LIBS})
endif (EXISTS ${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/lib/libxslt-static.a)

find_external_library(xerces INCLUDES xercesc/dom/DOM.hpp LIBS xerces-c HINTS "${NCBI_TOOLS_ROOT}/xerces-3.1.1/GCC442-DebugMT64")
#set(XERCES_STATIC_LIBS -L${NCBI_TOOLS_ROOT}/xerces-3.1.1/GCC442-DebugMT64/lib -lxerces-c-static -lcurl )

find_external_library(xalan INCLUDES xalanc/XalanTransformer/XalanTransformer.hpp LIBS xalan-c HINTS "${NCBI_TOOLS_ROOT}/xalan-1.11~r1302529/GCC442-DebugMT64" EXTRAFLAGS -lxalanMsg)
#set(XALAN_STATIC_LIBS  -L${NCBI_TOOLS_ROOT}/xalan-1.11~r1302529/GCC442-DebugMT64/lib -Wl,-rpath,/opt/ncbi/64/xalan-1.11~r1302529/GCC442-DebugMT64/lib:${NCBI_TOOLS_ROOT}/xalan-1.11~r1302529/GCC442-DebugMT64/lib -lxalan-c -lxalanMsg)

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
find_external_library(gsoap INCLUDES stdsoap2.h LIBS gsoapssl++ INCLUDE_HINTS "${NCBI_TOOLS_ROOT}/gsoap-2.8.15/include" LIBS_HINTS "${NCBI_TOOLS_ROOT}/gsoap-2.8.15/GCC442-DebugMT64/lib/" EXTRAFLAGS -lssl -lcrypto -lz)
set(GSOAP_SOAPCPP2 ${NCBI_TOOLS_ROOT}/gsoap-2.8.15/GCC442-DebugMT64/bin/soapcpp2)
set(GSOAP_WSDL2H   ${NCBI_TOOLS_ROOT}/gsoap-2.8.15/GCC442-DebugMT64/bin/wsdl2h)

# MongoDB
set(MONGODB_INCLUDE     ${NCBI_TOOLS_ROOT}/boost-1.53.0-ncbi1/include/boost-1_53 -Wno-unused-local-typedefs 
                        ${NCBI_TOOLS_ROOT}/mongodb-2.4.6/include)
set(MONGODB_LIBS        -L${NCBI_TOOLS_ROOT}/mongodb-2.4.6/lib -Wl,-rpath,/opt/ncbi/64/mongodb-2.4.6/lib:${NCBI_TOOLS_ROOT}/mongodb-2.4.6/lib -lmongoclient -L${NCBI_TOOLS_ROOT}/boost-1.53.0-ncbi1/lib -Wl,-rpath,/opt/ncbi/64/boost-1.53.0-ncbi1/lib:${NCBI_TOOLS_ROOT}/boost-1.53.0-ncbi1/lib -lboost_filesystem-gcc48-mt-d-1_53-64 -lboost_thread-gcc48-mt-d-1_53-64 -lboost_system-gcc48-mt-d-1_53-64)
set(MONGODB_STATIC_LIBS -L${NCBI_TOOLS_ROOT}/mongodb-2.4.6/lib -Wl,-rpath,/opt/ncbi/64/mongodb-2.4.6/lib:${NCBI_TOOLS_ROOT}/mongodb-2.4.6/lib -lmongodb -L${NCBI_TOOLS_ROOT}/boost-1.53.0-ncbi1/lib -Wl,-rpath,/opt/ncbi/64/boost-1.53.0-ncbi1/lib:${NCBI_TOOLS_ROOT}/boost-1.53.0-ncbi1/lib -lboost_filesystem-gcc48-mt-d-1_53-64-static -lboost_thread-gcc48-mt-d-1_53-64-static -lboost_system-gcc48-mt-d-1_53-64-static)

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
find_package(VDB)

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
find_external_library(samtools INCLUDES bam.h LIBS bam HINTS "${NCBI_TOOLS_ROOT}/samtools")

#LAPACK
set(LAPACK_LIBS "-llapack -lblas")

#LMBD
set(LMDB_INCLUDE "/netopt/ncbi_tools64/lmdb-0.9.18/include")
set(LMDB_LIBS -L/netopt/ncbi_tools64/lmdb-0.9.18/lib64 -llmdb)

find_external_library(libxlsxwriter INCLUDES xlsxwriter.h LIBS xlsxwriter HINTS "${NCBI_TOOLS_ROOT}/libxlsxwriter-0.6.9" EXTRAFLAGS "-lz")

##############################################################################
#
# NCBI-isms
# FIXME: these should be tested not hard-coded
set (NCBI_DATATOOL ${NCBI_TOOLS_ROOT}/bin/datatool)



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
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/corelib/ncbicfg.c.in ${CMAKE_CURRENT_SOURCE_DIR}/corelib/ncbicfg.c)

ENABLE_TESTING()
#include_directories(${CPP_INCLUDE_DIR} ${build_root}/inc)
include_directories(${incdir} ${includedir0} ${incinternal})

#
# Dump our final diagnostics
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.final-message.cmake)
