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


set(NCBI_COMPONENT_MSWin_FOUND YES)
#############################################################################
# common settings
set(NCBI_ThirdPartyBasePath //snowman/win-coremake/Lib/ThirdParty)
set(NCBI_ThirdPartyAppsPath //snowman/win-coremake/App/ThirdParty)
set(NCBI_PlatformBits 64)

if("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 15 2017 Win64")
  set(NCBI_ThirdPartyCompiler vs2015.64)
elseif("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 15 2017")
  if("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win64")
    set(NCBI_ThirdPartyCompiler vs2015.64)
  else()
    set(NCBI_ThirdPartyCompiler vs2015)
    set(NCBI_PlatformBits 32)
  endif()
elseif("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 14 2015 Win64")
  set(NCBI_ThirdPartyCompiler vs2015.64)
elseif("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 14 2015")
  if("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win64")
    set(NCBI_ThirdPartyCompiler vs2015.64)
  else()
    set(NCBI_ThirdPartyCompiler vs2015)
    set(NCBI_PlatformBits 32)
  endif()
else()
  message(FATAL_ERROR "${CMAKE_GENERATOR} not supported")
endif()


set(NCBI_ThirdParty_NCBI_C  //snowman/win-coremake/Lib/Ncbi/C/${NCBI_ThirdPartyCompiler}/c.current)

set(NCBI_ThirdParty_TLS        ${NCBI_ThirdPartyBasePath}/gnutls/${NCBI_ThirdPartyCompiler}/3.4.9)
set(NCBI_ThirdParty_FASTCGI    ${NCBI_ThirdPartyBasePath}/fastcgi/${NCBI_ThirdPartyCompiler}/2.4.0)
set(NCBI_ThirdParty_Boost      ${NCBI_ThirdPartyBasePath}/boost/${NCBI_ThirdPartyCompiler}/1.61.0)
set(NCBI_ThirdParty_PCRE       ${NCBI_ThirdPartyBasePath}/pcre/${NCBI_ThirdPartyCompiler}/7.9)
set(NCBI_ThirdParty_Z          ${NCBI_ThirdPartyBasePath}/z/${NCBI_ThirdPartyCompiler}/1.2.8)
set(NCBI_ThirdParty_BZ2        ${NCBI_ThirdPartyBasePath}/bzip2/${NCBI_ThirdPartyCompiler}/1.0.6)
set(NCBI_ThirdParty_LZO        ${NCBI_ThirdPartyBasePath}/lzo/${NCBI_ThirdPartyCompiler}/2.05)
set(NCBI_ThirdParty_BerkeleyDB ${NCBI_ThirdPartyBasePath}/berkeleydb/${NCBI_ThirdPartyCompiler}/4.6.21.NC)
set(NCBI_ThirdParty_LMDB       ${NCBI_ThirdPartyBasePath}/lmdb/${NCBI_ThirdPartyCompiler}/0.9.21)
set(NCBI_ThirdParty_JPEG       ${NCBI_ThirdPartyBasePath}/jpeg/${NCBI_ThirdPartyCompiler}/6b)
set(NCBI_ThirdParty_PNG        ${NCBI_ThirdPartyBasePath}/png/${NCBI_ThirdPartyCompiler}/1.2.7)
set(NCBI_ThirdParty_GIF        ${NCBI_ThirdPartyBasePath}/gif/${NCBI_ThirdPartyCompiler}/4.1.3)
set(NCBI_ThirdParty_TIFF       ${NCBI_ThirdPartyBasePath}/tiff/${NCBI_ThirdPartyCompiler}/3.6.1)
set(NCBI_ThirdParty_XML        ${NCBI_ThirdPartyBasePath}/xml/${NCBI_ThirdPartyCompiler}/2.7.8)
set(NCBI_ThirdParty_XSLT       ${NCBI_ThirdPartyBasePath}/xslt/${NCBI_ThirdPartyCompiler}/1.1.26)
set(NCBI_ThirdParty_EXSLT      ${NCBI_ThirdParty_XSLT})
set(NCBI_ThirdParty_SQLITE3    ${NCBI_ThirdPartyBasePath}/sqlite/${NCBI_ThirdPartyCompiler}/3.8.10.1)
set(NCBI_ThirdParty_Sybase     ${NCBI_ThirdPartyBasePath}/sybase/${NCBI_ThirdPartyCompiler}/15.5)
set(NCBI_ThirdParty_VDB        //snowman/trace_software/vdb/vdb-versions/cxx_toolkit/2)
if ("${NCBI_PlatformBits}" EQUAL "64")
  set(NCBI_ThirdParty_VDB_ARCH_INC x86_64)
  set(NCBI_ThirdParty_VDB_ARCH     x86_64/vs2013.64)
else()
  set(NCBI_ThirdParty_VDB_ARCH_INC i386)
  set(NCBI_ThirdParty_VDB_ARCH     i386/vs2013.32)
endif()

set(NCBI_ThirdParty_PYTHON     ${NCBI_ThirdPartyAppsPath}/Python252)


#############################################################################
macro(NCBI_define_component _name)
  if (DEFINED NCBI_ThirdParty_${_name})
    set(_root ${NCBI_ThirdParty_${_name}})
  else()
    string(FIND ${_name} "." dotfound)
    string(SUBSTRING ${_name} 0 ${dotfound} _dotname)
    if (DEFINED NCBI_ThirdParty_${_dotname})
      set(_root ${NCBI_ThirdParty_${_dotname}})
    else()
      message("Component ${_name} ERROR: NCBI_ThirdParty_${_name} not found")
    endif()
  endif()
  set(_args ${ARGN})
  if (EXISTS ${_root}/include)
    set(_found YES)
  else()
    message("Component ${_name} ERROR: ${_root}/include not found")
    set(_found NO)
  endif()
  if (_found)
    foreach(_testtype IN ITEMS lib_static lib_dll)
      set(_found YES)
      foreach(_cfg ${CMAKE_CONFIGURATION_TYPES})
        foreach(_lib IN LISTS _args)
          if(NOT EXISTS ${_root}/${_testtype}/${_cfg}/${_lib})
#            message("Component ${_name} ERROR: ${_root}/${_testtype}/${_cfg}/${_lib} not found")
            set(_found NO)
          endif()
        endforeach()
      endforeach()
      if (_found)
        set(_libtype ${_testtype})
        break()
      endif()
    endforeach()
    if (NOT _found)
      message("Component ${_name} ERROR: some libraries not found at ${_root}")
    endif()
  endif()
  if (_found)
    message("${_name} found at ${_root}")
    set(NCBI_COMPONENT_${_name}_FOUND YES)
    set(NCBI_COMPONENT_${_name}_INCLUDE ${_root}/include)
    foreach(_lib IN LISTS _args)
      set(NCBI_COMPONENT_${_name}_LIBS ${NCBI_COMPONENT_${_name}_LIBS} ${_root}/${_libtype}/\$\(Configuration\)/${_lib})
    endforeach()
    if (EXISTS ${_root}/bin)
      set(NCBI_COMPONENT_${_name}_BINPATH ${_root}/bin)
    endif()
#message("NCBI_COMPONENT_${_name}_INCLUDE ${NCBI_COMPONENT_${_name}_INCLUDE}")
#message("NCBI_COMPONENT_${_name}_LIBS ${NCBI_COMPONENT_${_name}_LIBS}")

    string(TOUPPER ${_name} _upname)
    set(HAVE_LIB${_upname} 1)
    string(REPLACE "." "_" _altname ${_upname})
    set(HAVE_${_altname} 1)

    set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} ${_name}")
  else()
    set(NCBI_COMPONENT_${_name}_FOUND NO)
  endif()
endmacro()

#############################################################################
# NCBI_C
if (EXISTS ${NCBI_ThirdParty_NCBI_C}/include)
  message("NCBI_C found at ${NCBI_ThirdParty_NCBI_C}")
  set(NCBI_COMPONENT_NCBI_C_FOUND YES)
  set(NCBI_COMPONENT_NCBI_C_INCLUDE ${NCBI_ThirdParty_NCBI_C}/include)

  set(_c_libs  blast ddvlib medarch ncbi ncbiacc ncbicdr
               ncbicn3d ncbicn3d_ogl ncbidesk ncbiid1
               ncbimain ncbimmdb ncbinacc ncbiobj ncbispel
               ncbitool ncbitxc2 netblast netcli netentr
               regexp smartnet vibgif vibnet vibrant
               vibrant_ogl)

  foreach( _lib IN LISTS _c_libs)
    set(NCBI_COMPONENT_NCBI_C_LIBS ${NCBI_COMPONENT_NCBI_C_LIBS} "${NCBI_ThirdParty_NCBI_C}/\$\(Configuration\)/${_lib}.lib")
  endforeach()
#  set(NCBI_COMPONENT_NCBI_C_DEFINES HAVE_NCBI_C=1)
else()
  message("Component NCBI_C ERROR: ${NCBI_ThirdParty_NCBI_C}/include not found")
  set(NCBI_COMPONENT_NCBI_C_FOUND NO)
endif()

#############################################################################
# STACKTRACE
set(NCBI_COMPONENT_STACKTRACE_FOUND YES)
set(NCBI_COMPONENT_STACKTRACE_LIBS dbghelp.lib)

#############################################################################
# TLS
if (EXISTS ${NCBI_ThirdParty_TLS}/include)
  message("TLS found at ${NCBI_ThirdParty_TLS}")
  set(NCBI_COMPONENT_TLS_FOUND YES)
  set(NCBI_COMPONENT_TLS_INCLUDE ${NCBI_ThirdParty_TLS}/include)
else()
  message("Component TLS ERROR: ${NCBI_ThirdParty_TLS}/include not found")
  set(NCBI_COMPONENT_TLS_FOUND NO)
endif()

#############################################################################
# FASTCGI
NCBI_define_component(FASTCGI libfcgi.lib)

#############################################################################
# Boost.Test.Included
if (EXISTS ${NCBI_ThirdParty_Boost}/include)
  message("Boost.Test.Included found at ${NCBI_ThirdParty_Boost}")
  set(NCBI_COMPONENT_Boost.Test.Included_FOUND YES)
  set(NCBI_COMPONENT_Boost.Test.Included_INCLUDE ${NCBI_ThirdParty_Boost}/include)
  set(NCBI_COMPONENT_Boost.Test.Included_DEFINES BOOST_TEST_NO_LIB)
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} Boost.Test.Included")
else()
  message("Component Boost.Test.Included ERROR: ${NCBI_ThirdParty_Boost}/include not found")
  set(NCBI_COMPONENT_Boost.Test.Included_FOUND NO)
endif()

#############################################################################
# Boost.Test
NCBI_define_component(Boost.Test libboost_unit_test_framework.lib)
if(NCBI_COMPONENT_Boost.Test_FOUND)
  set(NCBI_COMPONENT_Boost.Test_DEFINES BOOST_AUTO_LINK_NOMANGLE)
endif()

#############################################################################
# Boost.Spirit
NCBI_define_component(Boost.Spirit libboost_thread.lib boost_thread.lib boost_system.lib boost_date_time.lib boost_chrono.lib)
if(NCBI_COMPONENT_Boost.Spirit_FOUND)
  set(NCBI_COMPONENT_Boost.Spirit_DEFINES BOOST_AUTO_LINK_NOMANGLE)
endif()

#############################################################################
# PCRE
NCBI_define_component(PCRE libpcre.lib)
if(NOT NCBI_COMPONENT_PCRE_FOUND)
  set(NCBI_COMPONENT_PCRE_FOUND ${NCBI_COMPONENT_LocalPCRE_FOUND})
  set(NCBI_COMPONENT_PCRE_INCLUDE ${NCBI_COMPONENT_LocalPCRE_INCLUDE})
  set(NCBI_COMPONENT_PCRE_NCBILIB ${NCBI_COMPONENT_LocalPCRE_NCBILIB})
endif()

#############################################################################
# Z
NCBI_define_component(Z libz.lib)
if(NOT NCBI_COMPONENT_Z_FOUND)
  set(NCBI_COMPONENT_Z_FOUND ${NCBI_COMPONENT_LocalZ_FOUND})
  set(NCBI_COMPONENT_Z_INCLUDE ${NCBI_COMPONENT_LocalZ_INCLUDE})
  set(NCBI_COMPONENT_Z_NCBILIB ${NCBI_COMPONENT_LocalZ_NCBILIB})
endif()

#############################################################################
#BZ2
NCBI_define_component(BZ2 libbzip2.lib)
if(NOT NCBI_COMPONENT_BZ2_FOUND)
  set(NCBI_COMPONENT_BZ2_FOUND ${NCBI_COMPONENT_LocalBZ2_FOUND})
  set(NCBI_COMPONENT_BZ2_INCLUDE ${NCBI_COMPONENT_LocalBZ2_INCLUDE})
  set(NCBI_COMPONENT_BZ2_NCBILIB ${NCBI_COMPONENT_LocalBZ2_NCBILIB})
endif()

#############################################################################
#LZO
NCBI_define_component(LZO liblzo.lib)

#############################################################################
#BerkeleyDB
NCBI_define_component(BerkeleyDB libdb.lib)
if(NCBI_COMPONENT_BerkeleyDB_FOUND)
  set(HAVE_BERKELEY_DB 1)
  set(HAVE_BDB         1)
  set(HAVE_BDB_CACHE   1)
endif()

#############################################################################
#LMDB
NCBI_define_component(LMDB liblmdb.lib)
if(NOT NCBI_COMPONENT_LMDB_FOUND)
  set(NCBI_COMPONENT_LMDB_FOUND ${NCBI_COMPONENT_LocalLMDB_FOUND})
  set(NCBI_COMPONENT_LMDB_INCLUDE ${NCBI_COMPONENT_LocalLMDB_INCLUDE})
  set(NCBI_COMPONENT_LMDB_NCBILIB ${NCBI_COMPONENT_LocalLMDB_NCBILIB})
endif()

#############################################################################
# JPEG
NCBI_define_component(JPEG libjpeg.lib)

#############################################################################
# PNG
NCBI_define_component(PNG libpng.lib)

#############################################################################
# GIF
NCBI_define_component(GIF libgif.lib)

#############################################################################
# TIFF
NCBI_define_component(TIFF libtiff.lib)

#############################################################################
# XML
NCBI_define_component(XML libxml2.lib)
if (NCBI_COMPONENT_XML_FOUND)
  set (NCBI_COMPONENT_XML_DEFINES LIBXML_STATIC)
endif()

#############################################################################
# XSLT
NCBI_define_component(XSLT libexslt.lib libxslt.lib)

#############################################################################
# EXSLT
NCBI_define_component(EXSLT libexslt.lib)

#############################################################################
# SQLITE3
NCBI_define_component(SQLITE3 sqlite3.lib)

#############################################################################
# LAPACK
set(NCBI_COMPONENT_LAPACK_FOUND NO)

#############################################################################
# Sybase
NCBI_define_component(Sybase libsybdb.lib libsybct.lib libsybblk.lib libsybcs.lib)
if (NCBI_COMPONENT_Sybase_FOUND)
  set(SYBASE_PATH ${NCBI_ThirdParty_Sybase}/Sybase)
  set(SYBASE_LCL_PATH "")
endif()

#############################################################################
# MySQL
set(NCBI_COMPONENT_MySQL_FOUND NO)

#############################################################################
# ODBC
set(NCBI_COMPONENT_ODBC_FOUND YES)
set(NCBI_COMPONENT_ODBC_LIBS odbc32.lib odbccp32.lib odbcbcp.lib)
set(HAVE_ODBC 1)
set(HAVE_ODBCSS_H 1)

#############################################################################
# VDB
set(NCBI_COMPONENT_VDB_INCLUDE
  ${NCBI_ThirdParty_VDB}/interfaces
  ${NCBI_ThirdParty_VDB}/interfaces/cc/vc++/${NCBI_ThirdParty_VDB_ARCH_INC}
  ${NCBI_ThirdParty_VDB}/interfaces/cc/vc++
  ${NCBI_ThirdParty_VDB}/interfaces/os/win)
set(NCBI_COMPONENT_VDB_LIBS
  ${NCBI_ThirdParty_VDB}/win/release/${NCBI_ThirdParty_VDB_ARCH}/bin/ncbi-vdb-md.lib)
set(NCBI_COMPONENT_VDB_BINPATH
  ${NCBI_ThirdParty_VDB}/win/release/${NCBI_ThirdParty_VDB_ARCH}/bin)

set(_found YES)
foreach(_inc IN LISTS NCBI_COMPONENT_VDB_INCLUDE NCBI_COMPONENT_VDB_LIBS)
  if(NOT EXISTS ${_inc})
    message("Component VDB ERROR: ${_inc} not found")
    set(_found NO)
  endif()
endforeach()
if(_found)
  message("VDB found at ${NCBI_ThirdParty_VDB}")
  set(NCBI_COMPONENT_VDB_FOUND YES)
  set(HAVE_NCBI_VDB 1)
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} VDB")
else()
  set(NCBI_COMPONENT_VDB_FOUND NO)
  unset(NCBI_COMPONENT_VDB_INCLUDE)
  unset(NCBI_COMPONENT_VDB_LIBS)
endif()

#############################################################################
# PYTHON
NCBI_define_component(PYTHON)
