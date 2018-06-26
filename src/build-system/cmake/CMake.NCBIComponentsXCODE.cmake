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


set(NCBI_COMPONENT_XCODE_FOUND YES)
#############################################################################
# common settings
set(NCBI_ThirdPartyBasePath /netopt/ncbi_tools)

set(NCBI_ThirdParty_TLS        ${NCBI_ThirdPartyBasePath}/gnutls-3.4.0
#set(NCBI_ThirdParty_FASTCGI 
set(NCBI_ThirdParty_Boost      ${NCBI_ThirdPartyBasePath}/boost-1.62.0-ncbi1)
#set(NCBI_ThirdParty_PCRE
#set(NCBI_ThirdParty_Z
#set(NCBI_ThirdParty_BZ2
set(NCBI_ThirdParty_LZO        ${NCBI_ThirdPartyBasePath}/lzo-2.05)
set(NCBI_ThirdParty_BerkeleyDB ${NCBI_ThirdPartyBasePath}/BerkeleyDB)
set(NCBI_ThirdParty_LMDB       ${NCBI_ThirdPartyBasePath}/lmdb-0.9.18)
set(NCBI_ThirdParty_JPEG       ${NCBI_ThirdPartyBasePath}/safe-sw)
set(NCBI_ThirdParty_PNG        /opt/X11)
#set(NCBI_ThirdParty_GIF
set(NCBI_ThirdParty_TIFF       ${NCBI_ThirdPartyBasePath}/safe-sw)
set(NCBI_ThirdParty_XML        ${NCBI_ThirdPartyBasePath}/libxml-2.7.8
set(NCBI_ThirdParty_XSLT       ${NCBI_ThirdPartyBasePath}/libxml-2.7.8
set(NCBI_ThirdParty_EXSLT      ${NCBI_ThirdParty_XSLT})
set(NCBI_ThirdParty_SQLITE3    ${NCBI_ThirdPartyBasePath}/sqlite-3.8.10.1-ncbi1)
#set(NCBI_ThirdParty_Sybase

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
    set(_libtype lib)
    foreach(_lib IN LISTS _args)
      if(NOT EXISTS ${_root}/${_libtype}/${_lib})
        message("Component ${_name} ERROR: ${_root}/${_libtype}/${_lib} not found")
        set(_found NO)
      endif()
    endforeach()
  endif()
  if (_found)
    message("${_name} found at ${_root}")
    set(NCBI_COMPONENT_${_name}_FOUND YES)
    set(NCBI_COMPONENT_${_name}_INCLUDE ${_root}/include)
    foreach(_lib IN LISTS _args)
      set(NCBI_COMPONENT_${_name}_LIBS ${NCBI_COMPONENT_${_name}_LIBS} ${_root}/${_libtype}/${_lib})
    endforeach()
#message("NCBI_COMPONENT_${_name}_INCLUDE ${NCBI_COMPONENT_${_name}_INCLUDE}")
#message("NCBI_COMPONENT_${_name}_LIBS ${NCBI_COMPONENT_${_name}_LIBS}")

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
set(NCBI_COMPONENT_FASTCGI_FOUND NO)

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
# Boost.Test
NCBI_define_component(Boost.Test libboost_unit_test_framework.a)

#############################################################################
# Boost.Spirit
NCBI_define_component(Boost.Spirit libboost_thread-mt.a)

#############################################################################
# PCRE
if(NOT NCBI_COMPONENT_PCRE_FOUND)
  set(NCBI_COMPONENT_PCRE_FOUND ${NCBI_COMPONENT_LocalPCRE_FOUND})
  set(NCBI_COMPONENT_PCRE_INCLUDE ${NCBI_COMPONENT_LocalPCRE_INCLUDE})
  set(NCBI_COMPONENT_PCRE_LIBS ${NCBI_COMPONENT_LocalPCRE_LIBS})
endif()

#############################################################################
# Z
set(NCBI_COMPONENT_Z_FOUND YES)
set(NCBI_COMPONENT_Z_LIBS -lz)
set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} Z")

#############################################################################
#BZ2
set(NCBI_COMPONENT_BZ2_FOUND YES)
set(NCBI_COMPONENT_BZ2_LIBS -lbz2)
set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} BZ2")

#############################################################################
# LZO
NCBI_define_component(LZO liblzo2.a)

#############################################################################
#BerkeleyDB
NCBI_define_component(BerkeleyDB libdb.a)
if(NCBI_COMPONENT_BerkeleyDB_FOUND)
  set(HAVE_BERKELEY_DB 1)
  set(HAVE_BDB         1)
  set(HAVE_BDB_CACHE   1)
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

#############################################################################
# XML
NCBI_define_component(XML libxml2.a)
if(NCBI_COMPONENT_XML_FOUND)
  set(NCBI_COMPONENT_XML_INCLUDE ${NCBI_ThirdParty_XML}/include/libxml2)
endif()

#############################################################################
# XSLT
NCBI_define_component(XSLT libexslt.a libxslt.a)

#############################################################################
# EXSLT
NCBI_define_component(EXSLT libexslt.a)

#############################################################################
# SQLITE3
NCBI_define_component(SQLITE3 libsqlite3.a)

#############################################################################
#LAPACK
set(NCBI_COMPONENT_LAPACK_FOUND YES)
set(NCBI_COMPONENT_LAPACK_LIBS -llapack)
set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} LAPACK")

#############################################################################
# Sybase
set(NCBI_COMPONENT_Sybase_FOUND NO)

#############################################################################
# MySQL
set(NCBI_COMPONENT_MySQL_FOUND NO)

#############################################################################
# ODBC
set(NCBI_COMPONENT_ODBC_FOUND YES)
set(HAVE_ODBC 1)
set(HAVE_ODBCSS_H 1)

#############################################################################
# PYTHON
set(NCBI_COMPONENT_PYTHON_FOUND NO)
