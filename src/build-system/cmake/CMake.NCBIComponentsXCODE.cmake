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
set(NCBI_COMPONENT_unix_FOUND YES)
#############################################################################
# common settings
set(NCBI_TOOLS_ROOT $ENV{NCBI})
set(NCBI_OPT_ROOT  /opt/X11)
set(NCBI_PlatformBits 64)

include(CheckLibraryExists)
include(${NCBI_TREE_CMAKECFG}/FindExternalLibrary.cmake)

check_library_exists(dl dlopen "" HAVE_LIBDL)
if(HAVE_LIBDL)
    set(DL_LIBS -ldl)
else(HAVE_LIBDL)
    message(FATAL_ERROR "dl library not found")
endif(HAVE_LIBDL)

set(THREAD_LIBS   ${CMAKE_THREAD_LIBS_INIT})
find_library(CRYPT_LIBS NAMES crypt)
find_library(MATH_LIBS NAMES m)

if (APPLE)
    find_library(NETWORK_LIBS c)
    find_library(RT_LIBS c)
else (APPLE)
    find_library(NETWORK_LIBS nsl)
    find_library(RT_LIBS        rt)
endif (APPLE)
set(ORIG_LIBS   ${DL_LIBS} ${RT_LIBS} ${MATH_LIBS} ${CMAKE_THREAD_LIBS_INIT})

#############################################################################
# Kerberos 5
set(KRB5_LIBS "-framework Kerberos" -liconv)

############################################################################
set(NCBI_ThirdPartyBasePath ${NCBI_TOOLS_ROOT})

set(NCBI_ThirdParty_TLS        ${NCBI_ThirdPartyBasePath}/gnutls-3.4.0)
#set(NCBI_ThirdParty_FASTCGI 
set(NCBI_ThirdParty_Boost      ${NCBI_ThirdPartyBasePath}/boost-1.62.0-ncbi1)
#set(NCBI_ThirdParty_PCRE
#set(NCBI_ThirdParty_Z
#set(NCBI_ThirdParty_BZ2
set(NCBI_ThirdParty_LZO        ${NCBI_ThirdPartyBasePath}/lzo-2.05)
set(NCBI_ThirdParty_BerkeleyDB ${NCBI_ThirdPartyBasePath}/BerkeleyDB)
set(NCBI_ThirdParty_LMDB       ${NCBI_ThirdPartyBasePath}/lmdb-0.9.18)
set(NCBI_ThirdParty_JPEG       ${NCBI_ThirdPartyBasePath}/safe-sw)
set(NCBI_ThirdParty_PNG        ${NCBI_OPT_ROOT})
#set(NCBI_ThirdParty_GIF
set(NCBI_ThirdParty_TIFF       ${NCBI_ThirdPartyBasePath}/safe-sw)
set(NCBI_ThirdParty_XML        ${NCBI_ThirdPartyBasePath}/libxml-2.7.8)
set(NCBI_ThirdParty_XSLT       ${NCBI_ThirdPartyBasePath}/libxml-2.7.8)
set(NCBI_ThirdParty_EXSLT      ${NCBI_ThirdParty_XSLT})
set(NCBI_ThirdParty_SQLITE3    ${NCBI_ThirdPartyBasePath}/sqlite-3.8.10.1-ncbi1)
#set(NCBI_ThirdParty_Sybase
set(NCBI_ThirdParty_VDB        "/net/snowman/vol/projects/trace_software/vdb/vdb-versions/2.9.2-1")
set(NCBI_ThirdParty_VDB_ARCH x86_64)

#############################################################################
#############################################################################

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

    if (EXISTS ${_root}/${CMAKE_BUILD_TYPE}${NCBI_PlatformBits}/include)
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
        set(_suffixes .dylib .a)
    else()
        set(_suffixes .a .dylib)
    endif()
    set(_roots ${_root})
#    set(_subdirs ${CMAKE_BUILD_TYPE}${NCBI_PlatformBits}/lib lib64 lib)
    set(_subdirs ${CMAKE_BUILD_TYPE}${NCBI_PlatformBits}/lib lib64)
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
macro(NCBI_find_library _name)
    if(NOT DEFINED NCBI_COMPONENT_${_name}_FOUND)
        set(_args ${ARGN})
        find_library(NCBI_COMPONENT_${_name}_LIBS ${_args})
        if(NCBI_COMPONENT_${_name}_LIBS)
            set(NCBI_COMPONENT_${_name}_FOUND YES)
            list(APPEND NCBI_ALL_COMPONENTS ${_name})
            message(STATUS "Found ${_name}: ${NCBI_COMPONENT_${_name}_LIBS}")

            string(TOUPPER ${_name} _upname)
            set(HAVE_LIB${_upname} 1)
            set(HAVE_${_upname} 1)
        else()
            set(NCBI_COMPONENT_${_name}_FOUND NO)
            message("NOT FOUND ${_name}")
        endif()
    else()
        message("Already defined ${_name}")
    endif()
endmacro()

#############################################################################
# NCBI_C
set(NCBI_COMPONENT_NCBI_C_FOUND NO)

#############################################################################
# STACKTRACE
set(NCBI_COMPONENT_STACKTRACE_FOUND NO)

#############################################################################
#LMDB
NCBI_define_component(LMDB lmdb)
if(NOT NCBI_COMPONENT_LMDB_FOUND)
  set(NCBI_COMPONENT_LMDB_FOUND ${NCBI_COMPONENT_LocalLMDB_FOUND})
  set(NCBI_COMPONENT_LMDB_INCLUDE ${NCBI_COMPONENT_LocalLMDB_INCLUDE})
  set(NCBI_COMPONENT_LMDB_NCBILIB ${NCBI_COMPONENT_LocalLMDB_NCBILIB})
endif()

#############################################################################
# PCRE
if(NOT NCBI_COMPONENT_PCRE_FOUND)
  set(NCBI_COMPONENT_PCRE_FOUND ${NCBI_COMPONENT_LocalPCRE_FOUND})
  set(NCBI_COMPONENT_PCRE_INCLUDE ${NCBI_COMPONENT_LocalPCRE_INCLUDE})
  set(NCBI_COMPONENT_PCRE_NCBILIB ${NCBI_COMPONENT_LocalPCRE_NCBILIB})
  list(APPEND NCBI_ALL_COMPONENTS PCRE)
endif()

#############################################################################
# Z
set(NCBI_COMPONENT_Z_FOUND YES)
set(NCBI_COMPONENT_Z_LIBS -lz)
list(APPEND NCBI_ALL_COMPONENTS Z)

#############################################################################
#BZ2
set(NCBI_COMPONENT_BZ2_FOUND YES)
set(NCBI_COMPONENT_BZ2_LIBS -lbz2)
list(APPEND NCBI_ALL_COMPONENTS BZ2)

#############################################################################
# LZO
NCBI_define_component(LZO lzo2)

#############################################################################
# Boost.Test.Included
if (EXISTS ${NCBI_ThirdParty_Boost}/include)
  message(STATUS "Found Boost.Test.Included: ${NCBI_ThirdParty_Boost}")
  set(NCBI_COMPONENT_Boost.Test.Included_FOUND YES)
  set(NCBI_COMPONENT_Boost.Test.Included_INCLUDE ${NCBI_ThirdParty_Boost}/include)
  list(APPEND NCBI_ALL_COMPONENTS Boost.Test.Included)
else()
  message("Component Boost.Test.Included ERROR: ${NCBI_ThirdParty_Boost}/include not found")
  set(NCBI_COMPONENT_Boost.Test.Included_FOUND NO)
endif()

#############################################################################
# Boost.Test
NCBI_define_component(Boost.Test boost_unit_test_framework)

#############################################################################
# Boost.Spirit
NCBI_define_component(Boost.Spirit boost_thread-mt)

#############################################################################
# JPEG
NCBI_define_component(JPEG jpeg)

#############################################################################
# PNG
NCBI_define_component(PNG png)

#############################################################################
# GIF
set(NCBI_COMPONENT_GIF_FOUND YES)
list(APPEND NCBI_ALL_COMPONENTS GIF)

#############################################################################
# TIFF
NCBI_define_component(TIFF tiff)

#############################################################################
# TLS
if (EXISTS ${NCBI_ThirdParty_TLS}/include)
  message(STATUS "Found TLS: ${NCBI_ThirdParty_TLS}")
  set(NCBI_COMPONENT_TLS_FOUND YES)
  set(NCBI_COMPONENT_TLS_INCLUDE ${NCBI_ThirdParty_TLS}/include)
  list(APPEND NCBI_ALL_COMPONENTS TLS)
else()
  message("Component TLS ERROR: ${NCBI_ThirdParty_TLS}/include not found")
  set(NCBI_COMPONENT_TLS_FOUND NO)
endif()

#############################################################################
# FASTCGI
set(NCBI_COMPONENT_FASTCGI_FOUND NO)

#############################################################################
# SQLITE3
NCBI_define_component(SQLITE3 sqlite3)

#############################################################################
#BerkeleyDB
NCBI_define_component(BerkeleyDB db)
if(NCBI_COMPONENT_BerkeleyDB_FOUND)
  set(HAVE_BERKELEY_DB 1)
  set(HAVE_BDB         1)
  set(HAVE_BDB_CACHE   1)
endif()

#############################################################################
# ODBC
set(NCBI_COMPONENT_ODBC_FOUND NO)
set(ODBC_INCLUDE  ${NCBI_INC_ROOT}/dbapi/driver/odbc/unix_odbc 
                  ${NCBI_INC_ROOT}/dbapi/driver/odbc/unix_odbc)
set(NCBI_COMPONENT_ODBC_INCLUDE ${ODBC_INCLUDE})
set(HAVE_ODBC 1)
set(HAVE_ODBCSS_H 0)

#############################################################################
# MySQL
set(NCBI_COMPONENT_MySQL_FOUND NO)

#############################################################################
# Sybase
set(NCBI_COMPONENT_Sybase_FOUND NO)

#############################################################################
# PYTHON
set(NCBI_COMPONENT_PYTHON_FOUND NO)

#############################################################################
# VDB
set(NCBI_COMPONENT_VDB_INCLUDE
  ${NCBI_ThirdParty_VDB}/interfaces
  ${NCBI_ThirdParty_VDB}/interfaces/cc/gcc/${NCBI_ThirdParty_VDB_ARCH}
  ${NCBI_ThirdParty_VDB}/interfaces/cc/gcc
  ${NCBI_ThirdParty_VDB}/interfaces/os/mac
  ${NCBI_ThirdParty_VDB}/interfaces/os/unix)
set(NCBI_COMPONENT_VDB_LIBS
  ${NCBI_ThirdParty_VDB}/mac/release/${NCBI_ThirdParty_VDB_ARCH}/lib/libncbi-vdb.a)

set(_found YES)
foreach(_inc IN LISTS NCBI_COMPONENT_VDB_INCLUDE NCBI_COMPONENT_VDB_LIBS)
  if(NOT EXISTS ${_inc})
    message("Component VDB ERROR: ${_inc} not found")
    set(_found NO)
  endif()
endforeach()
if(_found)
  message(STATUS "Found VDB: ${NCBI_ThirdParty_VDB}")
  set(NCBI_COMPONENT_VDB_FOUND YES)
  set(HAVE_NCBI_VDB 1)
  list(APPEND NCBI_ALL_COMPONENTS VDB)
else()
  set(NCBI_COMPONENT_VDB_FOUND NO)
  unset(NCBI_COMPONENT_VDB_INCLUDE)
  unset(NCBI_COMPONENT_VDB_LIBS)
endif()

#############################################################################
# XML
NCBI_define_component(XML xml2)
if(NCBI_COMPONENT_XML_FOUND)
  set(NCBI_COMPONENT_XML_INCLUDE ${NCBI_ThirdParty_XML}/include/libxml2)
  set(NCBI_COMPONENT_XML_LIBS ${NCBI_COMPONENT_XML_LIBS} -liconv)
endif()

#############################################################################
# XSLT
NCBI_define_component(XSLT exslt xslt)

#############################################################################
# EXSLT
NCBI_define_component(EXSLT exslt)

#############################################################################
#LAPACK
set(NCBI_COMPONENT_LAPACK_FOUND YES)
set(NCBI_COMPONENT_LAPACK_LIBS -llapack)
list(APPEND NCBI_ALL_COMPONENTS LAPACK)
