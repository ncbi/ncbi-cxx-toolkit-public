#############################################################################
# $Id$
#############################################################################

##
## NCBI CMake components description - MSVC
##
##
## As a result, the following variables should be defined for component XXX
##  NCBI_COMPONENT_XXX_FOUND
##  NCBI_COMPONENT_XXX_INCLUDE
##  NCBI_COMPONENT_XXX_DEFINES
##  NCBI_COMPONENT_XXX_LIBS
##  HAVE_LIBXXX
##  HAVE_XXX


#to debug
#set(NCBI_TRACE_COMPONENT_GRPC ON)
#############################################################################
if("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 17 2022")
    set(NCBI_ThirdPartyCompiler vs2022.64)
elseif("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 16 2019")
    set(NCBI_ThirdPartyCompiler vs2019.64)
elseif("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 15 2017 Win64")
    set(NCBI_ThirdPartyCompiler vs2017.64)
elseif("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 15 2017")
    if("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "x64" OR "${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win64")
        set(NCBI_ThirdPartyCompiler vs2017.64)
    else()
        set(NCBI_ThirdPartyCompiler vs2017)
        set(NCBI_PlatformBits 32)
        message(FATAL_ERROR "32 bit configurations not supported")
    endif()
else()
#    message(WARNING "Generator ${CMAKE_GENERATOR} not tested")
    set(NCBI_ThirdPartyCompiler vs2022.64)
endif()

#############################################################################
# prebuilt libraries

set(NCBI_ThirdPartyBasePath ${NCBI_TOOLS_ROOT}/Lib/ThirdParty)
set(NCBI_ThirdPartyAppsPath ${NCBI_TOOLS_ROOT}/App/ThirdParty)
set(NCBI_ThirdParty_NCBI_C  ${NCBI_TOOLS_ROOT}/Lib/Ncbi/C/${NCBI_ThirdPartyCompiler}/c.sc-30)

set(NCBI_ThirdParty_VDBROOT    //snowman/trace_software/vdb)
set(NCBI_ThirdParty_SQLServer "C:/Program Files/Microsoft SQL Server/Client SDK/ODBC/180/SDK")
set(NCBI_ThirdParty_PYTHON     ${NCBI_ThirdPartyAppsPath}/Python_3.11 CACHE PATH "PYTHON root")
set(NCBI_ThirdParty_Sybase     ${NCBI_ThirdPartyBasePath}/sybase/${NCBI_ThirdPartyCompiler}/15.5 CACHE PATH "Sybase root")
set(NCBI_ThirdParty_SybaseLocalPath "" CACHE PATH "SybaseLocalPath")

if("${NCBI_ThirdPartyCompiler}" STREQUAL "vs2022.64")
    set(NCBI_ThirdParty_BerkeleyDB ${NCBI_ThirdPartyBasePath}/berkeleydb/${NCBI_ThirdPartyCompiler}/5.3.28-ncbi1 CACHE PATH "BerkeleyDB root")
    set(NCBI_ThirdParty_Boost      ${NCBI_ThirdPartyBasePath}/boost/${NCBI_ThirdPartyCompiler}/1.89.0 CACHE PATH "Boost root")
    set(NCBI_ThirdParty_BZ2        ${NCBI_ThirdPartyBasePath}/bzip2/${NCBI_ThirdPartyCompiler}/1.0.8-ncbi1 CACHE PATH "BZ2 root")
    set(NCBI_ThirdParty_FASTCGI    ${NCBI_ThirdPartyBasePath}/fastcgi/${NCBI_ThirdPartyCompiler}/2.4.1 CACHE PATH "FASTCGI root")
    set(NCBI_ThirdParty_FreeType   ${NCBI_ThirdPartyBasePath}/freetype/${NCBI_ThirdPartyCompiler}/2.14.1 CACHE PATH "FreeType root")
    set(NCBI_ThirdParty_FTGL       ${NCBI_ThirdPartyBasePath}/ftgl/${NCBI_ThirdPartyCompiler}/2.1.3_rc5 CACHE PATH "FTGL root")
    set(NCBI_ThirdParty_GIF        ${NCBI_ThirdPartyBasePath}/gif/${NCBI_ThirdPartyCompiler}/4.1.3 CACHE PATH "GIF root")
    set(NCBI_ThirdParty_GL2PS      ${NCBI_ThirdPartyBasePath}/gl2ps/${NCBI_ThirdPartyCompiler}/1.4.2 CACHE PATH "GL2PS root")
    set(NCBI_ThirdParty_GLEW       ${NCBI_ThirdPartyBasePath}/glew/${NCBI_ThirdPartyCompiler}/2.2.0 CACHE PATH "GLEW root")
    set(NCBI_ThirdParty_GNUTLS     ${NCBI_ThirdPartyBasePath}/gnutls/${NCBI_ThirdPartyCompiler}/3.8.9 CACHE PATH "GNUTLS root")
    set(NCBI_ThirdParty_PROTOBUF   ${NCBI_ThirdPartyBasePath}/grpc/${NCBI_ThirdPartyCompiler}/1.50.2-ncbi1 CACHE PATH "PROTOBUF root")
    set(NCBI_ThirdParty_GRPC       ${NCBI_ThirdPartyBasePath}/grpc/${NCBI_ThirdPartyCompiler}/1.50.2-ncbi1 CACHE PATH "GRPC root")
    set(NCBI_ThirdParty_JPEG       ${NCBI_ThirdPartyBasePath}/jpeg/${NCBI_ThirdPartyCompiler}/3.1.1 CACHE PATH "JPEG root")
    set(NCBI_ThirdParty_LMDB       ${NCBI_ThirdPartyBasePath}/lmdb/${NCBI_ThirdPartyCompiler}/0.9.33 CACHE PATH "LMDB root")
    set(NCBI_ThirdParty_LZO        ${NCBI_ThirdPartyBasePath}/lzo/${NCBI_ThirdPartyCompiler}/2.10-ncbi1 CACHE PATH "LZO root")
    set(NCBI_ThirdParty_NCBICRYPT  ${NCBI_ThirdPartyBasePath}/ncbicrypt/${NCBI_ThirdPartyCompiler}/1.0.1 CACHE PATH "NCBICRYPT root")
    set(NCBI_ThirdParty_NGHTTP2    ${NCBI_ThirdPartyBasePath}/nghttp2/${NCBI_ThirdPartyCompiler}/1.67.1 CACHE PATH "NGHTTP2 root")
    set(NCBI_ThirdParty_PCRE       ${NCBI_ThirdPartyBasePath}/pcre/${NCBI_ThirdPartyCompiler}/8.42 CACHE PATH "PCRE root")
    set(NCBI_ThirdParty_PCRE2      ${NCBI_ThirdPartyBasePath}/pcre2/${NCBI_ThirdPartyCompiler}/10.46 CACHE PATH "PCRE2 root")
    set(NCBI_ThirdParty_PNG        ${NCBI_ThirdPartyBasePath}/png/${NCBI_ThirdPartyCompiler}/1.6.50 CACHE PATH "PNG root")
    set(NCBI_ThirdParty_SQLITE3    ${NCBI_ThirdPartyBasePath}/sqlite/${NCBI_ThirdPartyCompiler}/3.26.0 CACHE PATH "SQLITE3 root")
    set(NCBI_ThirdParty_TIFF       ${NCBI_ThirdPartyBasePath}/tiff/${NCBI_ThirdPartyCompiler}/3.6.1-ncbi1 CACHE PATH "TIFF root")
    set(NCBI_ThirdParty_UV         ${NCBI_ThirdPartyBasePath}/uv/${NCBI_ThirdPartyCompiler}/1.51.0 CACHE PATH "UV root")
    set(NCBI_ThirdParty_XALAN      ${NCBI_ThirdPartyBasePath}/xalan/${NCBI_ThirdPartyCompiler}/1.12 CACHE PATH "XALAN root")
    set(NCBI_ThirdParty_XERCES     ${NCBI_ThirdPartyBasePath}/xerces/${NCBI_ThirdPartyCompiler}/3.2.3 CACHE PATH "XERCES root")
    set(NCBI_ThirdParty_XML        ${NCBI_ThirdPartyBasePath}/xml/${NCBI_ThirdPartyCompiler}/2.15.1 CACHE PATH "XML root")
    set(NCBI_ThirdParty_XSLT       ${NCBI_ThirdPartyBasePath}/xslt/${NCBI_ThirdPartyCompiler}/1.1.43 CACHE PATH "XSLT root")
    set(NCBI_ThirdParty_EXSLT      ${NCBI_ThirdParty_XSLT})
    set(NCBI_ThirdParty_Z          ${NCBI_ThirdPartyBasePath}/z/${NCBI_ThirdPartyCompiler}/1.2.12-ncbi1 CACHE PATH "Z root")
    set(NCBI_ThirdParty_ZSTD       ${NCBI_ThirdPartyBasePath}/zstd/${NCBI_ThirdPartyCompiler}/1.5.2 CACHE PATH "ZSTD root")
    set(NCBI_ThirdParty_VDB        ${NCBI_ThirdParty_VDBROOT}/vdb-versions/3.3.0 CACHE PATH "VDB root")
    set(NCBI_ThirdParty_VDB_ARCH_INC x86_64)
    set(NCBI_ThirdParty_VDB_ARCH x86_64/v143)

    if(NOT noUNICODE IN_LIST NCBI_PTBCFG_PROJECT_FEATURES AND NOT -UNICODE IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
        set(NCBI_ThirdParty_wxWidgets  ${NCBI_ThirdPartyBasePath}/wxwidgets/${NCBI_ThirdPartyCompiler}/3.2.1-ncbi1u CACHE PATH "wxWidgets root")
    else()
        set(NCBI_ThirdParty_wxWidgets  ${NCBI_ThirdPartyBasePath}/wxwidgets/${NCBI_ThirdPartyCompiler}/3.2.1-ncbi1 CACHE PATH "wxWidgets root")
    endif()

elseif("${NCBI_ThirdPartyCompiler}" STREQUAL "vs2019.64")

    set(NCBI_ThirdParty_BerkeleyDB ${NCBI_ThirdPartyBasePath}/berkeleydb/${NCBI_ThirdPartyCompiler}/5.3.28-ncbi1 CACHE PATH "BerkeleyDB root")
    set(NCBI_ThirdParty_Boost      ${NCBI_ThirdPartyBasePath}/boost/${NCBI_ThirdPartyCompiler}/1.76.0-ncbi1 CACHE PATH "Boost root")
    set(NCBI_ThirdParty_BZ2        ${NCBI_ThirdPartyBasePath}/bzip2/${NCBI_ThirdPartyCompiler}/1.0.8 CACHE PATH "BZ2 root")
    set(NCBI_ThirdParty_FASTCGI    ${NCBI_ThirdPartyBasePath}/fastcgi/${NCBI_ThirdPartyCompiler}/2.4.1 CACHE PATH "FASTCGI root")
    set(NCBI_ThirdParty_FreeType   ${NCBI_ThirdPartyBasePath}/freetype/${NCBI_ThirdPartyCompiler}/2.4.10 CACHE PATH "FreeType root")
    set(NCBI_ThirdParty_FTGL       ${NCBI_ThirdPartyBasePath}/ftgl/${NCBI_ThirdPartyCompiler}/2.1.3-rc5 CACHE PATH "FTGL root")
    set(NCBI_ThirdParty_GIF        ${NCBI_ThirdPartyBasePath}/gif/${NCBI_ThirdPartyCompiler}/4.1.3 CACHE PATH "GIF root")
    set(NCBI_ThirdParty_GL2PS      ${NCBI_ThirdPartyBasePath}/gl2ps/${NCBI_ThirdPartyCompiler}/1.4.0 CACHE PATH "GL2PS root")
    set(NCBI_ThirdParty_GLEW       ${NCBI_ThirdPartyBasePath}/glew/${NCBI_ThirdPartyCompiler}/1.5.8 CACHE PATH "GLEW root")
    set(NCBI_ThirdParty_GNUTLS     ${NCBI_ThirdPartyBasePath}/gnutls/${NCBI_ThirdPartyCompiler}/3.4.9 CACHE PATH "GNUTLS root")
    set(NCBI_ThirdParty_PROTOBUF   ${NCBI_ThirdPartyBasePath}/grpc/${NCBI_ThirdPartyCompiler}/1.50.2-ncbi1 CACHE PATH "PROTOBUF root")
    set(NCBI_ThirdParty_GRPC       ${NCBI_ThirdPartyBasePath}/grpc/${NCBI_ThirdPartyCompiler}/1.50.2-ncbi1 CACHE PATH "GRPC root")
    set(NCBI_ThirdParty_JPEG       ${NCBI_ThirdPartyBasePath}/jpeg/${NCBI_ThirdPartyCompiler}/9c CACHE PATH "JPEG root")
    set(NCBI_ThirdParty_LMDB       ${NCBI_ThirdPartyBasePath}/lmdb/${NCBI_ThirdPartyCompiler}/0.9.24 CACHE PATH "LMDB root")
    set(NCBI_ThirdParty_LZO        ${NCBI_ThirdPartyBasePath}/lzo/${NCBI_ThirdPartyCompiler}/2.10 CACHE PATH "LZO root")
    set(NCBI_ThirdParty_NCBICRYPT  ${NCBI_ThirdPartyBasePath}/ncbicrypt/${NCBI_ThirdPartyCompiler}/20230516 CACHE PATH "NCBICRYPT root")
    set(NCBI_ThirdParty_NGHTTP2    ${NCBI_ThirdPartyBasePath}/nghttp2/${NCBI_ThirdPartyCompiler}/1.40.0 CACHE PATH "NGHTTP2 root")
    set(NCBI_ThirdParty_PCRE       ${NCBI_ThirdPartyBasePath}/pcre/${NCBI_ThirdPartyCompiler}/8.42 CACHE PATH "PCRE root")
    set(NCBI_ThirdParty_PCRE2      ${NCBI_ThirdPartyBasePath}/pcre2/${NCBI_ThirdPartyCompiler}/10.44 CACHE PATH "PCRE2 root")
    set(NCBI_ThirdParty_PNG        ${NCBI_ThirdPartyBasePath}/png/${NCBI_ThirdPartyCompiler}/1.6.34 CACHE PATH "PNG root")
    set(NCBI_ThirdParty_SQLITE3    ${NCBI_ThirdPartyBasePath}/sqlite/${NCBI_ThirdPartyCompiler}/3.26.0 CACHE PATH "SQLITE3 root")
    set(NCBI_ThirdParty_TIFF       ${NCBI_ThirdPartyBasePath}/tiff/${NCBI_ThirdPartyCompiler}/3.6.1 CACHE PATH "TIFF root")
    set(NCBI_ThirdParty_UV         ${NCBI_ThirdPartyBasePath}/uv/${NCBI_ThirdPartyCompiler}/1.35.0.ncbi1 CACHE PATH "UV root")
    set(NCBI_ThirdParty_XALAN      ${NCBI_ThirdPartyBasePath}/xalan/${NCBI_ThirdPartyCompiler}/1.12 CACHE PATH "XALAN root")
    set(NCBI_ThirdParty_XERCES     ${NCBI_ThirdPartyBasePath}/xerces/${NCBI_ThirdPartyCompiler}/3.2.3 CACHE PATH "XERCES root")
    set(NCBI_ThirdParty_XML        ${NCBI_ThirdPartyBasePath}/xml/${NCBI_ThirdPartyCompiler}/2.7.8 CACHE PATH "XML root")
    set(NCBI_ThirdParty_XSLT       ${NCBI_ThirdPartyBasePath}/xslt/${NCBI_ThirdPartyCompiler}/1.1.26 CACHE PATH "XSLT root")
    set(NCBI_ThirdParty_EXSLT      ${NCBI_ThirdParty_XSLT})
    set(NCBI_ThirdParty_Z          ${NCBI_ThirdPartyBasePath}/z/${NCBI_ThirdPartyCompiler}/1.2.11 CACHE PATH "Z root")
    set(NCBI_ThirdParty_ZSTD       ${NCBI_ThirdPartyBasePath}/zstd/${NCBI_ThirdPartyCompiler}/1.5.2 CACHE PATH "ZSTD root")
    set(NCBI_ThirdParty_VDB        ${NCBI_ThirdParty_VDBROOT}/vdb-versions/3.3.0 CACHE PATH "VDB root")
    set(NCBI_ThirdParty_VDB_ARCH_INC x86_64)
    set(NCBI_ThirdParty_VDB_ARCH x86_64/v142)

    if(NOT noUNICODE IN_LIST NCBI_PTBCFG_PROJECT_FEATURES AND NOT -UNICODE IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
        set(NCBI_ThirdParty_wxWidgets  ${NCBI_ThirdPartyBasePath}/wxwidgets/${NCBI_ThirdPartyCompiler}/3.2.1-ncbi1u CACHE PATH "wxWidgets root")
    else()
        set(NCBI_ThirdParty_wxWidgets  ${NCBI_ThirdPartyBasePath}/wxwidgets/${NCBI_ThirdPartyCompiler}/3.2.1-ncbi1 CACHE PATH "wxWidgets root")
    endif()

elseif("${NCBI_ThirdPartyCompiler}" STREQUAL "vs2017.64")

    set(NCBI_ThirdParty_BerkeleyDB ${NCBI_ThirdPartyBasePath}/berkeleydb/${NCBI_ThirdPartyCompiler}/4.6.21.NC CACHE PATH "BerkeleyDB root")
    set(NCBI_ThirdParty_Boost      ${NCBI_ThirdPartyBasePath}/boost/${NCBI_ThirdPartyCompiler}/1.76.0-ncbi1 CACHE PATH "Boost root")
    set(NCBI_ThirdParty_BZ2        ${NCBI_ThirdPartyBasePath}/bzip2/${NCBI_ThirdPartyCompiler}/1.0.8 CACHE PATH "BZ2 root")
    set(NCBI_ThirdParty_FASTCGI    ${NCBI_ThirdPartyBasePath}/fastcgi/${NCBI_ThirdPartyCompiler}/2.4.1 CACHE PATH "FASTCGI root")
    set(NCBI_ThirdParty_FreeType   ${NCBI_ThirdPartyBasePath}/freetype/${NCBI_ThirdPartyCompiler}/2.4.10 CACHE PATH "FreeType root")
    set(NCBI_ThirdParty_FTGL       ${NCBI_ThirdPartyBasePath}/ftgl/${NCBI_ThirdPartyCompiler}/2.1.3-rc5 CACHE PATH "FTGL root")
    set(NCBI_ThirdParty_GIF        ${NCBI_ThirdPartyBasePath}/gif/${NCBI_ThirdPartyCompiler}/4.1.3 CACHE PATH "GIF root")
    set(NCBI_ThirdParty_GL2PS      ${NCBI_ThirdPartyBasePath}/gl2ps/${NCBI_ThirdPartyCompiler}/1.4.0 CACHE PATH "GL2PS root")
    set(NCBI_ThirdParty_GLEW       ${NCBI_ThirdPartyBasePath}/glew/${NCBI_ThirdPartyCompiler}/1.5.8 CACHE PATH "GLEW root")
    set(NCBI_ThirdParty_GNUTLS     ${NCBI_ThirdPartyBasePath}/gnutls/${NCBI_ThirdPartyCompiler}/3.4.9 CACHE PATH "GNUTLS root")
    set(NCBI_ThirdParty_PROTOBUF   ${NCBI_ThirdPartyBasePath}/grpc/${NCBI_ThirdPartyCompiler}/1.50.2-ncbi1 CACHE PATH "PROTOBUF root")
    set(NCBI_ThirdParty_GRPC       ${NCBI_ThirdPartyBasePath}/grpc/${NCBI_ThirdPartyCompiler}/1.50.2-ncbi1 CACHE PATH "GRPC root")
    set(NCBI_ThirdParty_JPEG       ${NCBI_ThirdPartyBasePath}/jpeg/${NCBI_ThirdPartyCompiler}/9c CACHE PATH "JPEG root")
    set(NCBI_ThirdParty_LMDB       ${NCBI_ThirdPartyBasePath}/lmdb/${NCBI_ThirdPartyCompiler}/0.9.24 CACHE PATH "LMDB root")
    set(NCBI_ThirdParty_LZO        ${NCBI_ThirdPartyBasePath}/lzo/${NCBI_ThirdPartyCompiler}/2.10 CACHE PATH "LZO root")
    # set(NCBI_ThirdParty_NCBICRYPT  ${NCBI_ThirdPartyBasePath}/ncbicrypt/${NCBI_ThirdPartyCompiler}/20230516 CACHE PATH "NCBICRYPT root")
    set(NCBI_ThirdParty_NGHTTP2    ${NCBI_ThirdPartyBasePath}/nghttp2/${NCBI_ThirdPartyCompiler}/1.40.0 CACHE PATH "NGHTTP2 root")
    set(NCBI_ThirdParty_PCRE       ${NCBI_ThirdPartyBasePath}/pcre/${NCBI_ThirdPartyCompiler}/8.42 CACHE PATH "PCRE root")
    set(NCBI_ThirdParty_PNG        ${NCBI_ThirdPartyBasePath}/png/${NCBI_ThirdPartyCompiler}/1.6.34 CACHE PATH "PNG root")
    set(NCBI_ThirdParty_SQLITE3    ${NCBI_ThirdPartyBasePath}/sqlite/${NCBI_ThirdPartyCompiler}/3.26.0 CACHE PATH "SQLITE3 root")
    set(NCBI_ThirdParty_TIFF       ${NCBI_ThirdPartyBasePath}/tiff/${NCBI_ThirdPartyCompiler}/3.6.1 CACHE PATH "TIFF root")
    set(NCBI_ThirdParty_UV         ${NCBI_ThirdPartyBasePath}/uv/${NCBI_ThirdPartyCompiler}/1.35.0 CACHE PATH "UV root")
    set(NCBI_ThirdParty_XALAN      ${NCBI_ThirdPartyBasePath}/xalan/${NCBI_ThirdPartyCompiler}/1.10.0-20080814 CACHE PATH "XALAN root")
    set(NCBI_ThirdParty_XERCES     ${NCBI_ThirdPartyBasePath}/xerces/${NCBI_ThirdPartyCompiler}/2.8.0 CACHE PATH "XERCES root")
    set(NCBI_ThirdParty_XML        ${NCBI_ThirdPartyBasePath}/xml/${NCBI_ThirdPartyCompiler}/2.7.8 CACHE PATH "XML root")
    set(NCBI_ThirdParty_XSLT       ${NCBI_ThirdPartyBasePath}/xslt/${NCBI_ThirdPartyCompiler}/1.1.26 CACHE PATH "XSLT root")
    set(NCBI_ThirdParty_EXSLT      ${NCBI_ThirdParty_XSLT})
    set(NCBI_ThirdParty_Z          ${NCBI_ThirdPartyBasePath}/z/${NCBI_ThirdPartyCompiler}/1.2.11 CACHE PATH "Z root")
    set(NCBI_ThirdParty_ZSTD       ${NCBI_ThirdPartyBasePath}/zstd/${NCBI_ThirdPartyCompiler}/1.5.2 CACHE PATH "ZSTD root")
    set(NCBI_ThirdParty_VDB        ${NCBI_ThirdParty_VDBROOT}/vdb-versions/3.3.0 CACHE PATH "VDB root")
    set(NCBI_ThirdParty_VDB_ARCH_INC x86_64)
    set(NCBI_ThirdParty_VDB_ARCH x86_64/v142)

    if(NOT noUNICODE IN_LIST NCBI_PTBCFG_PROJECT_FEATURES AND NOT -UNICODE IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
        set(NCBI_ThirdParty_wxWidgets  ${NCBI_ThirdPartyBasePath}/wxwidgets/${NCBI_ThirdPartyCompiler}/3.2.1-ncbi1u CACHE PATH "wxWidgets root")
    else()
        set(NCBI_ThirdParty_wxWidgets  ${NCBI_ThirdPartyBasePath}/wxwidgets/${NCBI_ThirdPartyCompiler}/3.2.1-ncbi1 CACHE PATH "wxWidgets root")
    endif()

else()
    message(FATAL_ERROR "Unknown compiler; cannot define 3rd party libraries")
endif()


#############################################################################
#############################################################################


#############################################################################
# in-house-resources et al.
if (NOT NCBI_COMPONENT_in-house-resources_DISABLED
        AND EXISTS "${NCBI_TOOLS_ROOT}/ncbi.ini"
        AND EXISTS "${NCBI_TOOLS_ROOT}/Scripts/test_data")
    set(NCBITEST_TESTDATA_PATH "${NCBI_TOOLS_ROOT}/Scripts/test_data")
    set(NCBI_REQUIRE_in-house-resources_FOUND YES)
    if(EXISTS "${NCBITEST_TESTDATA_PATH}/traces04")
        set(NCBI_REQUIRE_full-test-data_FOUND YES)
    endif()
    file(STRINGS "${NCBI_TOOLS_ROOT}/ncbi.ini" blastdb_line
         REGEX "^[Bb][Ll][Aa][Ss][Tt][Dd][Bb] *=")
    string(REGEX REPLACE "^[Bb][Ll][Aa][Ss][Tt][Dd][Bb] *= *" ""
           blastdb_path "${blastdb_line}")
    if(EXISTS "${blastdb_path}")
        set(NCBI_REQUIRE_full-blastdb_FOUND YES)
    endif()
endif()
NCBIcomponent_report(in-house-resources)
NCBIcomponent_report(full-test-data)
NCBIcomponent_report(full-blastdb)

#############################################################################
# NCBI_C
if(OFF)
  set(_c_libs  blast ddvlib medarch ncbi ncbiacc ncbicdr
               ncbicn3d ncbicn3d_ogl ncbidesk ncbiid1
               ncbimain ncbimmdb ncbinacc ncbiobj ncbispel
               ncbitool ncbitxc2 netblast netcli netentr
               regexp smartnet vibgif vibnet vibrant
               vibrant_ogl)
endif()
if(NOT NCBI_COMPONENT_NCBI_C_DISABLED)
    NCBI_define_Wcomponent(NCBI_C ncbiobj.lib ncbimmdb.lib ncbi.lib)
    if(NCBI_COMPONENT_NCBI_C_FOUND)
        set(NCBI_COMPONENT_NCBI_C_LIBPATH ${NCBI_ThirdParty_NCBI_C})
        set(NCBI_C_ncbi "ncbi")
    endif()
endif()
NCBIcomponent_report(NCBI_C)

##############################################################################
# UUID
set(NCBI_COMPONENT_UUID_FOUND YES)
set(NCBI_COMPONENT_UUID_LIBS uuid.lib rpcrt4.lib)

#############################################################################
# MySQL
set(NCBI_COMPONENT_MySQL_FOUND NO)

#############################################################################
# ODBC
if(NOT NCBI_COMPONENT_ODBC_DISABLED)
    set(NCBI_COMPONENT_ODBC_FOUND YES)
    set(NCBI_COMPONENT_ODBC_LIBS odbc32.lib odbccp32.lib
        # odbcbcp.lib
	)
    set(HAVE_ODBC 1)
    set(HAVE_ODBCSS_H 1)
endif()
NCBIcomponent_report(ODBC)

if(NOT NCBI_COMPONENT_SQLServer_DISABLED)
    NCBI_define_Wcomponent(SQLServer "x64/msodbcsql18.lib")
    if(NCBI_COMPONENT_SQLServer_FOUND)
        set(NCBI_COMPONENT_SQLServer_VERSION 180)
    endif()
endif()
NCBIcomponent_report(SQLServer)

##############################################################################
# OpenGL
if(NOT NCBI_COMPONENT_OpenGL_DISABLED)
    set(NCBI_COMPONENT_OpenGL_FOUND YES)
    set(NCBI_COMPONENT_OpenGL_LIBS opengl32.lib glu32.lib)
    set(HAVE_OPENGL 1)
endif()
NCBIcomponent_report(OpenGL)

#############################################################################
# LMDB
NCBI_define_Wcomponent(LMDB liblmdb.lib)
NCBIcomponent_report(LMDB)
if(NOT NCBI_COMPONENT_LMDB_FOUND)
    set(NCBI_COMPONENT_LMDB_FOUND ${NCBI_COMPONENT_LocalLMDB_FOUND})
    set(NCBI_COMPONENT_LMDB_INCLUDE ${NCBI_COMPONENT_LocalLMDB_INCLUDE})
    set(NCBI_COMPONENT_LMDB_NCBILIB ${NCBI_COMPONENT_LocalLMDB_NCBILIB})
endif()
set(HAVE_LIBLMDB ${NCBI_COMPONENT_LMDB_FOUND})

#############################################################################
# PCRE
if(NOT NCBI_COMPONENT_PCRE_FOUND)
    NCBI_define_Wcomponent(PCRE libpcre.lib)
    if(NCBI_COMPONENT_PCRE_FOUND)
        set(NCBI_COMPONENT_PCRE_DEFINES PCRE_STATIC NOPOSIX)
    endif()
endif()
NCBIcomponent_report(PCRE)
if(EXISTS ${NCBITK_INC_ROOT}/util/regexp/pcre.h
   AND NOT NCBI_COMPONENT_PCRE_FOUND)
    set(NCBI_COMPONENT_PCRE_FOUND ${NCBI_COMPONENT_LocalPCRE_FOUND})
    set(NCBI_COMPONENT_PCRE_INCLUDE ${NCBI_COMPONENT_LocalPCRE_INCLUDE})
    set(NCBI_COMPONENT_PCRE_NCBILIB ${NCBI_COMPONENT_LocalPCRE_NCBILIB})
endif()
set(HAVE_LIBPCRE ${NCBI_COMPONENT_PCRE_FOUND})

if(NOT NCBI_COMPONENT_PCRE2_FOUND)
    NCBI_define_Wcomponent(PCRE2 pcre2-8-static.lib)
    if(NCBI_COMPONENT_PCRE2_FOUND)
        set(NCBI_COMPONENT_PCRE2_DEFINES PCRE2_STATIC)
    endif()
endif()
NCBIcomponent_report(PCRE2)
if(EXISTS ${NCBITK_INC_ROOT}/util/regexp/pcre2.h
   AND NOT NCBI_COMPONENT_PCRE2_FOUND)
    set(NCBI_COMPONENT_PCRE2_FOUND ${NCBI_COMPONENT_LocalPCRE_FOUND})
    set(NCBI_COMPONENT_PCRE2_INCLUDE ${NCBI_COMPONENT_LocalPCRE_INCLUDE})
    set(NCBI_COMPONENT_PCRE2_NCBILIB ${NCBI_COMPONENT_LocalPCRE_NCBILIB})
endif()
set(HAVE_LIBPCRE2 ${NCBI_COMPONENT_PCRE2_FOUND})

#############################################################################
# Z
NCBI_define_Wcomponent(Z libz.lib)
NCBIcomponent_report(Z)
if(NOT NCBI_COMPONENT_Z_FOUND)
    set(NCBI_COMPONENT_Z_FOUND ${NCBI_COMPONENT_LocalZ_FOUND})
    set(NCBI_COMPONENT_Z_INCLUDE ${NCBI_COMPONENT_LocalZ_INCLUDE})
    set(NCBI_COMPONENT_Z_NCBILIB ${NCBI_COMPONENT_LocalZ_NCBILIB})
endif()
set(HAVE_LIBZ ${NCBI_COMPONENT_Z_FOUND})

#############################################################################
# BZ2
if("${NCBI_ThirdPartyCompiler}" STREQUAL "vs2022.64")
    NCBI_define_Wcomponent(BZ2 bz2_static.lib)
else()
    NCBI_define_Wcomponent(BZ2 libbzip2.lib)
endif()
NCBIcomponent_report(BZ2)
if(NOT NCBI_COMPONENT_BZ2_FOUND)
    set(NCBI_COMPONENT_BZ2_FOUND ${NCBI_COMPONENT_LocalBZ2_FOUND})
    set(NCBI_COMPONENT_BZ2_INCLUDE ${NCBI_COMPONENT_LocalBZ2_INCLUDE})
    set(NCBI_COMPONENT_BZ2_NCBILIB ${NCBI_COMPONENT_LocalBZ2_NCBILIB})
endif()
set(HAVE_LIBBZ2 ${NCBI_COMPONENT_BZ2_FOUND})

#############################################################################
# LZO
if("${NCBI_ThirdPartyCompiler}" STREQUAL "vs2022.64")
    NCBI_define_Wcomponent(LZO lzo2.lib)
else()
    NCBI_define_Wcomponent(LZO liblzo.lib)
endif()
NCBIcomponent_report(LZO)

#############################################################################
# ZSTD
NCBI_define_Wcomponent(ZSTD libzstd_static.lib)
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
#include(${NCBI_TREE_CMAKECFG}/CMakeChecks.boost.cmake)

#############################################################################
# Boost.Test.Included
if(NOT NCBI_COMPONENT_Boost.Test.Included_FOUND)
    NCBI_define_Wcomponent(Boost.Test.Included)
    if(NCBI_COMPONENT_Boost.Test.Included_FOUND)
        set(NCBI_COMPONENT_Boost.Test.Included_DEFINES BOOST_TEST_NO_LIB)
    endif()
endif()

#############################################################################
# Boost.Test
if(NOT NCBI_COMPONENT_Boost.Test_FOUND)
    NCBI_define_Wcomponent(Boost.Test libboost_unit_test_framework.lib)
    if(NCBI_COMPONENT_Boost.Test_FOUND)
        set(NCBI_COMPONENT_Boost.Test_DEFINES BOOST_AUTO_LINK_SYSTEM)
    endif()
endif()

#############################################################################
# Boost.Spirit
if(NOT NCBI_COMPONENT_Boost.Spirit_FOUND)
    if("${NCBI_ThirdPartyCompiler}" STREQUAL "vs2022.64")
        NCBI_define_Wcomponent(Boost.Spirit libboost_thread.lib libboost_date_time.lib libboost_chrono.lib)
    else()
        NCBI_define_Wcomponent(Boost.Spirit libboost_thread.lib boost_thread.lib boost_system.lib boost_date_time.lib boost_chrono.lib)
    endif()
    if(NCBI_COMPONENT_Boost.Spirit_FOUND)
        set(NCBI_COMPONENT_Boost.Spirit_DEFINES BOOST_AUTO_LINK_SYSTEM)
    endif()
endif()

#############################################################################
# Boost.Thread
if(NOT NCBI_COMPONENT_Boost.Thread_FOUND)
    if("${NCBI_ThirdPartyCompiler}" STREQUAL "vs2022.64")
        NCBI_define_Wcomponent(Boost.Thread libboost_thread.lib libboost_date_time.lib libboost_chrono.lib)
    else()
        NCBI_define_Wcomponent(Boost.Thread libboost_thread.lib boost_thread.lib boost_system.lib boost_date_time.lib boost_chrono.lib)
    endif()
    if(NCBI_COMPONENT_Boost.Thread_FOUND)
        set(NCBI_COMPONENT_Boost.Thread_DEFINES BOOST_AUTO_LINK_SYSTEM)
    endif()
endif()

#############################################################################
# Boost
if(NOT NCBI_COMPONENT_Boost_FOUND)
    if("${NCBI_ThirdPartyCompiler}" STREQUAL "vs2022.64")
        NCBI_define_Wcomponent(Boost libboost_filesystem.lib libboost_iostreams.lib libboost_date_time.lib libboost_regex.lib)
    else()
        NCBI_define_Wcomponent(Boost boost_filesystem.lib boost_iostreams.lib boost_date_time.lib boost_regex.lib boost_system.lib)
    endif()
endif()

endif(NOT NCBI_COMPONENT_Boost_DISABLED AND NOT NCBI_COMPONENT_Boost_FOUND)
NCBIcomponent_report(Boost.Test.Included)
NCBIcomponent_report(Boost.Test)
NCBIcomponent_report(Boost.Spirit)
NCBIcomponent_report(Boost.Thread)
NCBIcomponent_report(Boost)

#############################################################################
# JPEG
if("${NCBI_ThirdPartyCompiler}" STREQUAL "vs2022.64")
    NCBI_define_Wcomponent(JPEG jpeg-static.lib)
else()
    NCBI_define_Wcomponent(JPEG libjpeg.lib)
endif()
NCBIcomponent_report(JPEG)

#############################################################################
# PNG
NCBI_define_Wcomponent(PNG libpng.lib)
NCBIcomponent_report(PNG)

#############################################################################
# GIF
NCBI_define_Wcomponent(GIF libgif.lib)
NCBIcomponent_report(GIF)

#############################################################################
# TIFF
NCBI_define_Wcomponent(TIFF libtiff.lib)
NCBIcomponent_report(TIFF)

#############################################################################
# GNUTLS
NCBI_define_Wcomponent(GNUTLS libgnutls-30.lib)
NCBIcomponent_report(GNUTLS)

#############################################################################
# NCBICRYPT
NCBI_define_Wcomponent(NCBICRYPT ncbicrypt.lib)
NCBIcomponent_report(NCBICRYPT)

#############################################################################
# FASTCGI
NCBI_define_Wcomponent(FASTCGI libfcgi.lib)
NCBIcomponent_report(FASTCGI)

#############################################################################
# BerkeleyDB
NCBI_define_Wcomponent(BerkeleyDB libdb.lib)
if(NCBI_COMPONENT_BerkeleyDB_FOUND)
    set(HAVE_BERKELEY_DB 1)
    set(HAVE_BDB         1)
    set(HAVE_BDB_CACHE   1)
endif()
NCBIcomponent_report(BerkeleyDB)

#############################################################################
# XML
if(NOT NCBI_COMPONENT_XML_FOUND)
    NCBI_define_Wcomponent(XML libxml2.lib)
    if (NCBI_COMPONENT_XML_FOUND)
        if(NOT BUILD_SHARED_LIBS)
            set (NCBI_COMPONENT_XML_DEFINES LIBXML_STATIC)
        endif()
    endif()
endif()
NCBIcomponent_report(XML)

#############################################################################
# XSLT
if(NOT NCBI_COMPONENT_XSLT_FOUND)
    NCBI_define_Wcomponent(XSLT libexslt.lib libxslt.lib)
    if (NCBI_COMPONENT_XSLT_FOUND)
        if(NOT BUILD_SHARED_LIBS)
            set (NCBI_COMPONENT_XSLT_DEFINES LIBEXSLT_STATIC LIBXSLT_STATIC)
        endif()
    endif()
endif()
NCBIcomponent_report(XSLT)

#############################################################################
# EXSLT
if(NOT NCBI_COMPONENT_EXSLT_FOUND)
    NCBI_define_Wcomponent(EXSLT libexslt.lib)
    if (NCBI_COMPONENT_EXSLT_FOUND)
        if(NOT BUILD_SHARED_LIBS)
            set (NCBI_COMPONENT_EXSLT_DEFINES LIBEXSLT_STATIC)
        endif()
    endif()
endif()
NCBIcomponent_report(EXSLT)

#############################################################################
# SQLITE3
NCBI_define_Wcomponent(SQLITE3 sqlite3.lib)
NCBIcomponent_report(SQLITE3)

#############################################################################
# LAPACK
set(NCBI_COMPONENT_LAPACK_FOUND NO)

#############################################################################
# Sybase
NCBI_define_Wcomponent(Sybase # libsybdb.lib
                       libsybct.lib libsybblk.lib libsybcs.lib)
if (NCBI_COMPONENT_Sybase_FOUND)
    set(SYBASE_PATH ${NCBI_ThirdParty_Sybase}/Sybase)
    set(SYBASE_LCL_PATH "${NCBI_ThirdParty_SybaseLocalPath}")
endif()
NCBIcomponent_report(Sybase)

#############################################################################
# VDB
if(NOT NCBI_COMPONENT_VDB_DISABLED AND NOT NCBI_COMPONENT_VDB_FOUND)
    set(NCBI_COMPONENT_VDB_INCLUDE
        ${NCBI_ThirdParty_VDB}/interfaces
        ${NCBI_ThirdParty_VDB}/interfaces/cc/vc++/${NCBI_ThirdParty_VDB_ARCH_INC}
        ${NCBI_ThirdParty_VDB}/interfaces/cc/vc++
        ${NCBI_ThirdParty_VDB}/interfaces/os/win)

    foreach(_cfg IN LISTS NCBI_CONFIGURATION_TYPES)
        if(${_cfg} IN_LIST NCBI_DEBUG_CONFIGURATION_TYPES)
            set(NCBI_COMPONENT_VDB_BINPATH_${_cfg}
                ${NCBI_ThirdParty_VDB}/win/debug/${NCBI_ThirdParty_VDB_ARCH}/bin)
        else()
            set(NCBI_COMPONENT_VDB_BINPATH_${_cfg}
                ${NCBI_ThirdParty_VDB}/win/release/${NCBI_ThirdParty_VDB_ARCH}/bin)
        endif()
    endforeach()
    if(NOT "${NCBI_DEBUG_CONFIGURATION_TYPES}" STREQUAL "")
        set(NCBI_COMPONENT_VDB_BINPATH
            ${NCBI_ThirdParty_VDB}/win/$<IF:$<CONFIG:${NCBI_DEBUG_CONFIGURATION_TYPES_STRING}>,debug,release>/${NCBI_ThirdParty_VDB_ARCH}/bin)
    else()
        set(NCBI_COMPONENT_VDB_BINPATH
            ${NCBI_ThirdParty_VDB}/win/release/${NCBI_ThirdParty_VDB_ARCH}/bin)
    endif()
    set(NCBI_COMPONENT_VDB_LIBS
        ${NCBI_COMPONENT_VDB_BINPATH}/ncbi-vdb-md.lib)

    set(_found YES)
    foreach(_inc IN LISTS NCBI_COMPONENT_VDB_INCLUDE)
        if(NOT EXISTS ${_inc})
            NCBI_notice("NOT FOUND VDB: ${_inc} not found")
            set(_found NO)
        endif()
    endforeach()
    if(_found)
        message(STATUS "Found VDB: ${NCBI_ThirdParty_VDB}")
        set(NCBI_COMPONENT_VDB_FOUND YES)
        set(HAVE_NCBI_VDB 1)
    else()
        set(NCBI_COMPONENT_VDB_FOUND NO)
        unset(NCBI_COMPONENT_VDB_INCLUDE)
        unset(NCBI_COMPONENT_VDB_LIBS)
    endif()
endif()
NCBIcomponent_report(VDB)

#############################################################################
# PYTHON
NCBI_define_Wcomponent(PYTHON python311.lib python3.lib)
if(NCBI_COMPONENT_PYTHON_FOUND)
    set(NCBI_COMPONENT_PYTHON_BINPATH ${NCBI_ThirdParty_PYTHON})
    set(NCBI_COMPONENT_PYTHON_VERSION 3.11)
endif()
NCBIcomponent_report(PYTHON)

##############################################################################
# GRPC/PROTOBUF
if(NOT NCBI_PROTOC_APP)
    if(EXISTS "${NCBI_ThirdParty_GRPC}/bin/ReleaseDLL/protoc.exe")
        set(NCBI_PROTOC_APP "${NCBI_ThirdParty_GRPC}/bin/ReleaseDLL/protoc.exe")
    endif()
endif()
if(NOT NCBI_GRPC_PLUGIN)
    if(EXISTS "${NCBI_ThirdParty_GRPC}/bin/ReleaseDLL/grpc_cpp_plugin.exe")
        set(NCBI_GRPC_PLUGIN "${NCBI_ThirdParty_GRPC}/bin/ReleaseDLL/grpc_cpp_plugin.exe")
    endif()
endif()
if(NOT EXISTS "${NCBI_PROTOC_APP}")
    NCBI_notice("NOT FOUND NCBI_PROTOC_APP: ${NCBI_PROTOC_APP}")
else()
NCBI_define_Wcomponent(PROTOBUF
    libprotobuf.lib absl_random_distributions.lib
    absl_random_seed_sequences.lib absl_log_internal_check_op.lib
    absl_leak_check.lib absl_die_if_null.lib absl_log_internal_conditions.lib
    absl_log_internal_message.lib absl_examine_stack.lib
    absl_log_internal_format.lib absl_log_internal_proto.lib
    absl_log_internal_nullguard.lib absl_log_internal_log_sink_set.lib
    absl_log_sink.lib absl_log_entry.lib absl_flags.lib
    absl_flags_internal.lib absl_flags_marshalling.lib
    absl_flags_reflection.lib absl_flags_private_handle_accessor.lib
    absl_flags_commandlineflag.lib absl_flags_commandlineflag_internal.lib
    absl_flags_config.lib absl_flags_program_name.lib absl_log_initialize.lib
    absl_log_globals.lib absl_log_internal_globals.lib absl_raw_hash_set.lib
    absl_hash.lib absl_city.lib absl_low_level_hash.lib
    absl_hashtablez_sampler.lib absl_statusor.lib absl_status.lib
    absl_cord.lib absl_cordz_info.lib absl_cord_internal.lib
    absl_cordz_functions.lib absl_exponential_biased.lib
    absl_cordz_handle.lib absl_crc_cord_state.lib absl_crc32c.lib
    absl_crc_internal.lib absl_crc_cpu_detect.lib
    absl_bad_optional_access.lib absl_str_format_internal.lib
    absl_strerror.lib absl_synchronization.lib absl_graphcycles_internal.lib
    absl_kernel_timeout_internal.lib absl_stacktrace.lib absl_symbolize.lib
    absl_debugging_internal.lib absl_demangle_internal.lib
    absl_malloc_internal.lib absl_time.lib absl_civil_time.lib
    absl_time_zone.lib absl_bad_variant_access.lib utf8_validity.lib
    utf8_range.lib absl_strings.lib absl_string_view.lib
    absl_strings_internal.lib absl_base.lib absl_spinlock_wait.lib
    absl_int128.lib absl_throw_delegate.lib absl_raw_logging_internal.lib
    absl_log_severity.lib
)
if(NOT NCBI_COMPONENT_GRPC_FOUND)
    NCBI_define_Wcomponent(GRPC
        grpc++.lib grpc.lib gpr.lib address_sorting.lib cares.lib
        libprotoc.lib upb.lib boringcrypto.lib boringssl.lib re2.lib
        absl_random_internal_pool_urbg.lib absl_random_internal_randen.lib
        absl_random_internal_randen_hwaes.lib
        absl_random_internal_randen_hwaes_impl.lib
        absl_random_internal_randen_slow.lib absl_random_internal_platform.lib
        absl_random_internal_seed_material.lib absl_random_seed_gen_exception.lib
    )
    if(NCBI_COMPONENT_GRPC_FOUND)
        set(NCBI_COMPONENT_GRPC_DEFINES _WIN32_WINNT=0x0600)
        set(NCBI_COMPONENT_GRPC_LIBS
            ${NCBI_COMPONENT_GRPC_LIBS} ${NCBI_COMPONENT_PROTOBUF_LIBS})
    endif()
endif()
endif()
NCBIcomponent_report(PROTOBUF)
NCBIcomponent_report(GRPC)
if(NCBI_TRACE_COMPONENT_PROTOBUF OR NCBI_TRACE_ALLCOMPONENTS)
    message("NCBI_PROTOC_APP = ${NCBI_PROTOC_APP}")
endif()
if(NCBI_TRACE_COMPONENT_GRPC OR NCBI_TRACE_ALLCOMPONENTS)
    message("NCBI_GRPC_PLUGIN = ${NCBI_GRPC_PLUGIN}")
endif()

##############################################################################
# XALAN
NCBI_define_Wcomponent(XALAN xalan-c.lib XalanMsgLib.lib)
NCBIcomponent_report(XALAN)

##############################################################################
# XERCES
if(NOT NCBI_COMPONENT_XERCES_FOUND)
    NCBI_define_Wcomponent(XERCES xerces-c.lib)
    if(NCBI_COMPONENT_XERCES_FOUND)
        if(BUILD_SHARED_LIBS)
            set(NCBI_COMPONENT_XERCES_DEFINES XERCES_DLL)
        else()
            set(NCBI_COMPONENT_XERCES_DEFINES XML_LIBRARY)
        endif()
    endif()
endif()
NCBIcomponent_report(XERCES)

##############################################################################
# FTGL
if(NOT NCBI_COMPONENT_FTGL_FOUND)
    NCBI_define_Wcomponent(FTGL ftgl.lib)
    if(NCBI_COMPONENT_FTGL_FOUND)
        if(NOT BUILD_SHARED_LIBS)
            set(NCBI_COMPONENT_FTGL_DEFINES FTGL_LIBRARY_STATIC)
        endif()
    endif()
endif()
NCBIcomponent_report(FTGL)

##############################################################################
# FreeType
if(NOT NCBI_COMPONENT_FreeType_FOUND)
    NCBI_define_Wcomponent(FreeType freetype.lib)
    if(NCBI_COMPONENT_FreeType_FOUND)
        if(BUILD_SHARED_LIBS)
            set(NCBI_COMPONENT_FreeType_DEFINES DLL_IMPORT)
        endif()
    endif()
endif()
NCBIcomponent_report(FreeType)

##############################################################################
# GLEW
if("${NCBI_ThirdPartyCompiler}" STREQUAL "vs2022.64")

if(NOT NCBI_COMPONENT_GLEW_FOUND)
    NCBI_define_Wcomponent(GLEW libglew32.lib)
    if(NCBI_COMPONENT_GLEW_FOUND)
        if(NOT BUILD_SHARED_LIBS)
            set(NCBI_COMPONENT_GLEW_DEFINES GLEW_STATIC)
        endif()
    endif()
endif()

else()

if(NOT NCBI_COMPONENT_GLEW_FOUND)
    NCBI_define_Wcomponent(GLEW glew32mx.lib)
    if(NCBI_COMPONENT_GLEW_FOUND)
        if(BUILD_SHARED_LIBS)
            set(NCBI_COMPONENT_GLEW_DEFINES GLEW_MX)
        else()
            set(NCBI_COMPONENT_GLEW_DEFINES GLEW_MX GLEW_STATIC)
        endif()
    endif()
endif()

endif()
NCBIcomponent_report(GLEW)

##############################################################################
# wxWidgets
if(NOT NCBI_COMPONENT_wxWidgets_FOUND)
    NCBI_define_Wcomponent( wxWidgets
            wxbase.lib wxbase_net.lib wxbase_xml.lib wxmsw_core.lib wxmsw_gl.lib
            wxmsw_html.lib wxmsw_aui.lib wxmsw_adv.lib wxmsw_richtext.lib wxmsw_propgrid.lib
            wxmsw_xrc.lib wxexpat.lib wxjpeg.lib wxpng.lib wxregex.lib wxtiff.lib wxzlib.lib)
    if(NCBI_COMPONENT_wxWidgets_FOUND)
        set(NCBI_COMPONENT_wxWidgets_INCLUDE ${NCBI_COMPONENT_wxWidgets_INCLUDE} ${NCBI_COMPONENT_wxWidgets_INCLUDE}/msvc)
        if(BUILD_SHARED_LIBS)
            set(NCBI_COMPONENT_wxWidgets_DEFINES __WXMSW__ NCBI_WXWIN_USE_PCH WXUSINGDLL=1)
        else()
            set(NCBI_COMPONENT_wxWidgets_DEFINES __WXMSW__ NCBI_WXWIN_USE_PCH)
        endif()
    endif()
endif()
NCBIcomponent_report(wxWidgets)

##############################################################################
# UV
if(NOT NCBI_COMPONENT_UV_FOUND)
    NCBI_define_Wcomponent(UV libuv.lib)
    if(NCBI_COMPONENT_UV_FOUND)
        if(BUILD_SHARED_LIBS)
            set(NCBI_COMPONENT_UV_DEFINES USING_UV_SHARED)
        endif()
        set(NCBI_COMPONENT_UV_LIBS ${NCBI_COMPONENT_UV_LIBS} psapi.lib Iphlpapi.lib userenv.lib)
    endif()
endif()
NCBIcomponent_report(UV)

##############################################################################
# NGHTTP2
if(NOT NCBI_COMPONENT_NGHTTP2_FOUND)
    NCBI_define_Wcomponent(NGHTTP2 nghttp2.lib)
    if(NCBI_COMPONENT_NGHTTP2_FOUND)
        if(NOT BUILD_SHARED_LIBS)
            set(NCBI_COMPONENT_NGHTTP2_DEFINES NGHTTP2_STATICLIB)
        endif()
    endif()
endif()
NCBIcomponent_report(NGHTTP2)

##############################################################################
# GL2PS
NCBI_define_Wcomponent(GL2PS gl2ps.lib)
NCBIcomponent_report(GL2PS)
