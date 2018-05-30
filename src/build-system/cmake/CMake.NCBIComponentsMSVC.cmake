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


set(NCBI_ALL_COMPONENTS "")
set(NCBI_COMPONENT_MSWin_FOUND YES)
#############################################################################
# common settings
set(NCBI_ThirdPartyBasePath //snowman/win-coremake/Lib/ThirdParty)

if("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 15 2017 Win64")
  set(NCBI_ThirdPartyCompiler vs2015.64)
elseif("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 15 2017")
  if("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win64")
    set(NCBI_ThirdPartyCompiler vs2015.64)
  else()
    set(NCBI_ThirdPartyCompiler vs2015)
  endif()
elseif("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 14 2015 Win64")
  set(NCBI_ThirdPartyCompiler vs2015.64)
elseif("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 14 2015")
  if("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win64")
    set(NCBI_ThirdPartyCompiler vs2015.64)
  else()
    set(NCBI_ThirdPartyCompiler vs2015)
  endif()
else()
  message(FATAL_ERROR "${CMAKE_GENERATOR} not supported")
endif()


set(NCBI_ThirdParty_NCBI_C  //snowman/win-coremake/Lib/Ncbi/C/${NCBI_ThirdPartyCompiler}/c.current)

set(NCBI_ThirdParty_TLS     ${NCBI_ThirdPartyBasePath}/gnutls/${NCBI_ThirdPartyCompiler}/3.4.9)
set(NCBI_ThirdParty_FASTCGI ${NCBI_ThirdPartyBasePath}/fastcgi/${NCBI_ThirdPartyCompiler}/2.4.0)
set(NCBI_ThirdParty_Boost   ${NCBI_ThirdPartyBasePath}/boost/${NCBI_ThirdPartyCompiler}/1.61.0)
set(NCBI_ThirdParty_PCRE    ${NCBI_ThirdPartyBasePath}/pcre/${NCBI_ThirdPartyCompiler}/7.9)
set(NCBI_ThirdParty_Z       ${NCBI_ThirdPartyBasePath}/z/${NCBI_ThirdPartyCompiler}/1.2.8)
set(NCBI_ThirdParty_BZ2     ${NCBI_ThirdPartyBasePath}/bzip2/${NCBI_ThirdPartyCompiler}/1.0.6)
set(NCBI_ThirdParty_LZO     ${NCBI_ThirdPartyBasePath}/lzo/${NCBI_ThirdPartyCompiler}/2.05)
set(NCBI_ThirdParty_LMDB    ${NCBI_ThirdPartyBasePath}/lmdb/${NCBI_ThirdPartyCompiler}/0.9.21)
set(NCBI_ThirdParty_JPEG    ${NCBI_ThirdPartyBasePath}/jpeg/${NCBI_ThirdPartyCompiler}/6b)
set(NCBI_ThirdParty_PNG     ${NCBI_ThirdPartyBasePath}/png/${NCBI_ThirdPartyCompiler}/1.2.7)
set(NCBI_ThirdParty_GIF     ${NCBI_ThirdPartyBasePath}/gif/${NCBI_ThirdPartyCompiler}/4.1.3)
set(NCBI_ThirdParty_TIFF    ${NCBI_ThirdPartyBasePath}/tiff/${NCBI_ThirdPartyCompiler}/3.6.1)


#############################################################################
macro(NCBI_define_component _name _lib)
  set(_root ${NCBI_ThirdParty_${_name}})
  if (EXISTS ${_root}/include)
    set(_found YES)
  else()
    message("Component ${_name} ERROR: ${_root}/include not found")
    set(_found NO)
  endif()
  if (_found)
    set(_libtype lib_static)
    foreach(_cfg ${CMAKE_CONFIGURATION_TYPES})
      if(NOT EXISTS ${_root}/${_libtype}/${_cfg}/${_lib})
        message("Component ${_name} ERROR: ${_root}/${_libtype}/${_cfg}/${_lib} not found")
        set(_found NO)
      endif()
    endforeach()
  endif()
  if (_found)
    message("${_name} found at ${_root}")
    set(NCBI_COMPONENT_${_name}_FOUND YES)
    set(NCBI_COMPONENT_${_name}_INCLUDE ${_root}/include)
    set(NCBI_COMPONENT_${_name}_LIBS ${_root}/${_libtype}/\$\(Configuration\)/${_lib})
    string(TOUPPER ${_name} _upname)
    set(HAVE_LIB${_upname} 1)
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
  set(NCBI_COMPONENT_NCBI_C_DEFINES HAVE_NCBI_C=1)
else()
  message("Component NCBI_C ERROR: ${NCBI_ThirdParty_NCBI_C}/include not found")
  set(NCBI_COMPONENT_NCBI_C_FOUND NO)
endif()

#############################################################################
# local_lbsm
set(NCBI_COMPONENT_local_lbsm_FOUND NO)

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
#LocalPCRE
if (EXISTS ${includedir}/util/regexp)
  set(NCBI_COMPONENT_LocalPCRE_FOUND YES)
  set(NCBI_COMPONENT_LocalPCRE_INCLUDE ${includedir}/util/regexp)
  set(NCBI_COMPONENT_LocalPCRE_LIBS regexp)
else()
  set(NCBI_COMPONENT_LocalPCRE_FOUND NO)
endif()

#############################################################################
# PCRE
NCBI_define_component(PCRE libpcre.lib)
if(NOT NCBI_COMPONENT_PCRE_FOUND)
  set(NCBI_COMPONENT_PCRE_FOUND ${NCBI_COMPONENT_LocalPCRE_FOUND})
  set(NCBI_COMPONENT_PCRE_INCLUDE ${NCBI_COMPONENT_LocalPCRE_INCLUDE})
  set(NCBI_COMPONENT_PCRE_LIBS ${NCBI_COMPONENT_LocalPCRE_LIBS})
endif()

#############################################################################
#LocalZ
if (EXISTS ${includedir}/util/compress/zlib)
  set(NCBI_COMPONENT_LocalZ_FOUND YES)
  set(NCBI_COMPONENT_LocalZ_INCLUDE ${includedir}/util/compress/zlib)
  set(NCBI_COMPONENT_LocalZ_LIBS z)
else()
  set(NCBI_COMPONENT_LocalZ_FOUND NO)
endif()

#############################################################################
# Z
NCBI_define_component(Z libz.lib)
if(NOT NCBI_COMPONENT_Z_FOUND)
  set(NCBI_COMPONENT_Z_FOUND ${NCBI_COMPONENT_LocalZ_FOUND})
  set(NCBI_COMPONENT_Z_INCLUDE ${NCBI_COMPONENT_LocalZ_INCLUDE})
  set(NCBI_COMPONENT_Z_LIBS ${NCBI_COMPONENT_LocalZ_LIBS})
endif()

#############################################################################
#LocalBZ2
if (EXISTS ${includedir}/util/compress/bzip2)
  set(NCBI_COMPONENT_LocalBZ2_FOUND YES)
  set(NCBI_COMPONENT_LocalBZ2_INCLUDE ${includedir}/util/compress/bzip2)
  set(NCBI_COMPONENT_LocalBZ2_LIBS bz2)
else()
  set(NCBI_COMPONENT_LocalBZ2_FOUND NO)
endif()

#############################################################################
#BZ2
NCBI_define_component(BZ2 libbzip2.lib)
if(NOT NCBI_COMPONENT_BZ2_FOUND)
  set(NCBI_COMPONENT_BZ2_FOUND ${NCBI_COMPONENT_LocalBZ2_FOUND})
  set(NCBI_COMPONENT_BZ2_INCLUDE ${NCBI_COMPONENT_LocalBZ2_INCLUDE})
  set(NCBI_COMPONENT_BZ2_LIBS ${NCBI_COMPONENT_LocalBZ2_LIBS})
endif()

#############################################################################
#LZO
NCBI_define_component(LZO liblzo.lib)

#############################################################################
#LocalLMDB
if (EXISTS ${includedir}/util/lmdb)
  set(NCBI_COMPONENT_LocalLMDB_FOUND YES)
  set(NCBI_COMPONENT_LocalLMDB_INCLUDE ${includedir}//util/lmdb)
  set(NCBI_COMPONENT_LocalLMDB_LIBS lmdb)
else()
  set(NCBI_COMPONENT_LocalLMDB_FOUND NO)
endif()

#############################################################################
#LMDB
NCBI_define_component(LMDB liblmdb.lib)
if(NOT NCBI_COMPONENT_LMDB_FOUND)
  set(NCBI_COMPONENT_LMDB_FOUND ${NCBI_COMPONENT_LocalLMDB_FOUND})
  set(NCBI_COMPONENT_LMDB_INCLUDE ${NCBI_COMPONENT_LocalLMDB_INCLUDE})
  set(NCBI_COMPONENT_LMDB_LIBS ${NCBI_COMPONENT_LocalLMDB_LIBS})
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


