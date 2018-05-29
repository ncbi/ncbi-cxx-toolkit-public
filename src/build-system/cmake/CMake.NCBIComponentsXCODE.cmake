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


set(NCBI_ALL_COMPONENTS "")
#############################################################################
# common settings
set(NCBI_ThirdPartyBasePath /netopt/ncbi_tools)

set(NCBI_ThirdParty_TLS   ${NCBI_ThirdPartyBasePath}/gnutls-3.4.0
set(NCBI_ThirdParty_Boost ${NCBI_ThirdPartyBasePath}/boost-1.62.0-ncbi1)
set(NCBI_ThirdParty_LZO   ${NCBI_ThirdPartyBasePath}/lzo-2.05)
set(NCBI_ThirdParty_LMDB  ${NCBI_ThirdPartyBasePath}/lmdb-0.9.18)
set(NCBI_ThirdParty_JPEG  ${NCBI_ThirdPartyBasePath}/safe-sw)
set(NCBI_ThirdParty_PNG   /opt/X11)
set(NCBI_ThirdParty_TIFF  ${NCBI_ThirdPartyBasePath}/safe-sw)


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
    set(_libtype lib)
    if(NOT EXISTS ${_root}/${_libtype}/${_lib})
      message("Component ${_name} ERROR: ${_root}/${_libtype}/${_lib} not found")
      set(_found NO)
    endif()
  endif()
  if (_found)
    message("${_name} found at ${_root}")
    set(NCBI_COMPONENT_${_name}_FOUND YES)
    set(NCBI_COMPONENT_${_name}_INCLUDE ${_root}/include)
    set(NCBI_COMPONENT_${_name}_LIBS ${_root}/${_libtype}/${_lib})
    string(TOUPPER ${_name} _upname)
    set(HAVE_LIB${_upname} 1)
    set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} ${_name}")
  else()
    set(NCBI_COMPONENT_${_name}_FOUND NO)
  endif()
endmacro()

#############################################################################
# NCBI_C
set(NCBI_COMPONENT_NCBI_C_FOUND NO)

#############################################################################
# local_lbsm
if (EXISTS ${NCBI_SRC_ROOT}/src/connect/ncbi_lbsm.c)
  message("local_lbsm found at ${NCBI_SRC_ROOT}/src/connect")
  set(NCBI_COMPONENT_local_lbsm_FOUND YES)
  set(HAVE_LOCAL_LBSM 1)
else()
  message("Component local_lbsm ERROR: not found")
  set(NCBI_COMPONENT_local_lbsm_FOUND NO)
endif()

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
# Boost.Test.Included
if (EXISTS ${NCBI_ThirdParty_Boost}/include)
  message("Boost.Test.Included found at ${NCBI_ThirdParty_Boost}")
  set(NCBI_COMPONENT_Boost.Test.Included_FOUND YES)
  set(NCBI_COMPONENT_Boost.Test.Included_INCLUDE ${NCBI_ThirdParty_Boost}/include)
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
set(NCBI_COMPONENT_Z_FOUND YES)
set(NCBI_COMPONENT_Z_LIBS -lz)
set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} Z")

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
set(NCBI_COMPONENT_BZ2_FOUND YES)
set(NCBI_COMPONENT_BZ2_LIBS -lbz2)
set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} BZ2")

#############################################################################
# LZO
NCBI_define_component(LZO liblzo2.a)

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
NCBI_define_component(LMDB liblmdb.a)
if(NOT NCBI_COMPONENT_LMDB_FOUND)
  set(NCBI_COMPONENT_LMDB_FOUND ${NCBI_COMPONENT_LocalLMDB_FOUND})
  set(NCBI_COMPONENT_LMDB_INCLUDE ${NCBI_COMPONENT_LocalLMDB_INCLUDE})
  set(NCBI_COMPONENT_LMDB_LIBS ${NCBI_COMPONENT_LocalLMDB_LIBS})
endif()

#############################################################################
# JPEG
NCBI_define_component(JPEG libjpeg.a)

#############################################################################
# PNG
NCBI_define_component(PNG libpng.dylib)

#############################################################################
# GIF
set(NCBI_COMPONENT_GIF_FOUND YES)
set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} GIF")

#############################################################################
# TIFF
NCBI_define_component(TIFF libtiff.a)


