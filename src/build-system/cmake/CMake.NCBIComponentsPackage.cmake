#############################################################################
# $Id$
#############################################################################

##
## NCBI CMake components description - download/build using Conan
##
##
## As a result, the following variables should be defined for component XXX
##  NCBI_COMPONENT_XXX_FOUND
##  NCBI_COMPONENT_XXX_INCLUDE
##  NCBI_COMPONENT_XXX_DEFINES
##  NCBI_COMPONENT_XXX_LIBS
##  HAVE_LIBXXX
##  HAVE_XXX


set(NCBI_TRACE_ALLCOMPONENTS TRUE)
conan_define_targets()
#############################################################################
function(NCBI_define_Pkgcomponent)
    cmake_parse_arguments(DC "" "NAME;PACKAGE" "" ${ARGN})

    if("${DC_NAME}" STREQUAL "")
        message(FATAL_ERROR "No component name")
    endif()
    if("${DC_PACKAGE}" STREQUAL "")
        message(FATAL_ERROR "No package name")
    endif()
    set(_package ${DC_PACKAGE})
    string(TOUPPER ${DC_PACKAGE} DC_PACKAGE)
    if(NCBI_PTBCFG_COMPONENT_StaticComponents)
        set(_suffixes .a .so)
    else()
        if(BUILD_SHARED_LIBS OR TRUE)
            set(_suffixes .so .a)
        else()
            set(_suffixes .a .so)
        endif()
    endif()

    set(NCBI_COMPONENT_${DC_NAME}_FOUND NO PARENT_SCOPE)
    if(NCBI_COMPONENT_${DC_NAME}_DISABLED)
        message("DISABLED ${DC_NAME}")
    elseif(DEFINED CONAN_${DC_PACKAGE}_ROOT)
        set(NCBI_COMPONENT_${DC_NAME}_FOUND YES PARENT_SCOPE)
        set(_include ${CONAN_INCLUDE_DIRS_${DC_PACKAGE}})
        set(_defines ${CONAN_DEFINES_${DC_PACKAGE}})
        set(NCBI_COMPONENT_${DC_NAME}_INCLUDE ${_include} PARENT_SCOPE)
        set(NCBI_COMPONENT_${DC_NAME}_DEFINES ${_defines} PARENT_SCOPE)
        if(TARGET CONAN_PKG::${_package})
            set(_all_libs CONAN_PKG::${_package})
        else()
            set(_all_libs "")
            if(NOT "${CONAN_LIB_DIRS_${DC_PACKAGE}}" STREQUAL "" AND NOT "${CONAN_LIBS_${DC_PACKAGE}}" STREQUAL "")
                foreach(_lib IN LISTS CONAN_LIBS_${DC_PACKAGE})
                    set(_this_found NO)
                    foreach(_dir IN LISTS CONAN_LIB_DIRS_${DC_PACKAGE})
                        foreach(_sfx IN LISTS _suffixes)
                            if(EXISTS ${_dir}/lib${_lib}${_sfx})
                                list(APPEND _all_libs ${_dir}/lib${_lib}${_sfx})
                                set(_this_found YES)
                                if(NCBI_TRACE_COMPONENT_${DC_NAME} OR NCBI_TRACE_ALLCOMPONENTS)
                                    message("${DC_NAME}: found:  ${_dir}/lib${_lib}${_sfx}")
                                endif()
                                break()
                            endif()
                        endforeach()
                        if(_this_found)
                            break()
                        endif()
                    endforeach()
                    if(NOT _this_found)
                        list(APPEND _all_libs ${_lib})
                    endif()
                endforeach()
            endif()
        endif()
        set(NCBI_COMPONENT_${DC_NAME}_LIBS ${_all_libs} PARENT_SCOPE)
        string(TOUPPER ${DC_NAME} _upname)
        set(HAVE_LIB${_upname} 1 PARENT_SCOPE)
        string(REPLACE "." "_" _altname ${_upname})
        set(HAVE_${_altname} 1 PARENT_SCOPE)

        list(APPEND NCBI_ALL_COMPONENTS ${DC_NAME})
        set(NCBI_ALL_COMPONENTS ${NCBI_ALL_COMPONENTS} PARENT_SCOPE)
        if(NCBI_TRACE_COMPONENT_${DC_NAME} OR NCBI_TRACE_ALLCOMPONENTS)
            message("----------------------")
            message("NCBI_define_Pkgcomponent: ${DC_NAME}")
            message("include: ${_include}")
            message("libs:    ${_all_libs}")
            message("defines: ${_defines}")
            message("----------------------")
        endif()
    else()
        message("NOT FOUND ${DC_NAME}")
    endif()
endfunction()

#############################################################################
#############################################################################

#############################################################################
# BACKWARD, UNWIND
NCBI_define_Pkgcomponent(NAME BACKWARD PACKAGE backward-cpp)
list(REMOVE_ITEM NCBI_ALL_COMPONENTS BACKWARD)
if(NCBI_COMPONENT_BACKWARD_FOUND)
    set(HAVE_LIBBACKWARD_CPP YES)
endif()
#NCBI_define_Pkgcomponent(NAME UNWIND PACKAGE libunwind)
#list(REMOVE_ITEM NCBI_ALL_COMPONENTS UNWIND)

#############################################################################
# LMDB
if(NOT NCBI_COMPONENT_LMDB_FOUND)
    set(NCBI_COMPONENT_LMDB_FOUND ${NCBI_COMPONENT_LocalLMDB_FOUND})
    set(NCBI_COMPONENT_LMDB_INCLUDE ${NCBI_COMPONENT_LocalLMDB_INCLUDE})
    set(NCBI_COMPONENT_LMDB_NCBILIB ${NCBI_COMPONENT_LocalLMDB_NCBILIB})
    set(HAVE_LIBLMDB ${NCBI_COMPONENT_LMDB_FOUND})
endif()

#############################################################################
# PCRE
NCBI_define_Pkgcomponent(NAME PCRE PACKAGE pcre)
if(NOT NCBI_COMPONENT_PCRE_FOUND)
    set(NCBI_COMPONENT_PCRE_FOUND ${NCBI_COMPONENT_LocalPCRE_FOUND})
    set(NCBI_COMPONENT_PCRE_INCLUDE ${NCBI_COMPONENT_LocalPCRE_INCLUDE})
    set(NCBI_COMPONENT_PCRE_NCBILIB ${NCBI_COMPONENT_LocalPCRE_NCBILIB})
    set(HAVE_LIBPCRE ${NCBI_COMPONENT_PCRE_FOUND})
endif()

#############################################################################
# Z
NCBI_define_Pkgcomponent(NAME Z PACKAGE zlib)
if(NOT NCBI_COMPONENT_Z_FOUND)
    set(NCBI_COMPONENT_Z_FOUND ${NCBI_COMPONENT_LocalZ_FOUND})
    set(NCBI_COMPONENT_Z_INCLUDE ${NCBI_COMPONENT_LocalZ_INCLUDE})
    set(NCBI_COMPONENT_Z_NCBILIB ${NCBI_COMPONENT_LocalZ_NCBILIB})
    set(HAVE_LIBZ ${NCBI_COMPONENT_Z_FOUND})
endif()

#############################################################################
# BZ2
NCBI_define_Pkgcomponent(NAME BZ2 PACKAGE bzip2)
if(NOT NCBI_COMPONENT_BZ2_FOUND)
    set(NCBI_COMPONENT_BZ2_FOUND ${NCBI_COMPONENT_LocalBZ2_FOUND})
    set(NCBI_COMPONENT_BZ2_INCLUDE ${NCBI_COMPONENT_LocalBZ2_INCLUDE})
    set(NCBI_COMPONENT_BZ2_NCBILIB ${NCBI_COMPONENT_LocalBZ2_NCBILIB})
    set(HAVE_LIBBZ2 ${NCBI_COMPONENT_BZ2_FOUND})
endif()

#############################################################################
# Boost
NCBI_define_Pkgcomponent(NAME Boost PACKAGE boost)

#############################################################################
# Boost.Test.Included
if(NCBI_COMPONENT_Boost_FOUND)
  set(NCBI_COMPONENT_Boost.Test.Included_FOUND YES)
  set(NCBI_COMPONENT_Boost.Test.Included_INCLUDE ${NCBI_COMPONENT_Boost_INCLUDE})
  set(NCBI_COMPONENT_Boost.Test.Included_DEFINES BOOST_TEST_NO_LIB)
else()
  set(NCBI_COMPONENT_Boost.Test.Included_FOUND NO)
endif()

#############################################################################
# Boost.Test
if(NCBI_COMPONENT_Boost_FOUND)
  set(NCBI_COMPONENT_Boost.Test_FOUND YES)
  set(NCBI_COMPONENT_Boost.Test_INCLUDE ${NCBI_COMPONENT_Boost_INCLUDE})
  set(NCBI_COMPONENT_Boost.Test_LIBS    ${NCBI_COMPONENT_Boost_LIBS})
else()
  set(NCBI_COMPONENT_Boost.Test_FOUND NO)
endif()

#############################################################################
# Boost.Spirit
if(Boost_FOUND)
  set(NCBI_COMPONENT_Boost.Spirit_FOUND YES)
  set(NCBI_COMPONENT_Boost.Spirit_INCLUDE ${NCBI_COMPONENT_Boost_INCLUDE})
#  set(NCBI_COMPONENT_Boost.Spirit_LIBS    ${Boost_LIBRARIES})
else()
  set(NCBI_COMPONENT_Boost.Spirit_FOUND NO)
endif()

#############################################################################
# Boost.Thread
if(Boost_FOUND)
  set(NCBI_COMPONENT_Boost.Thread_FOUND YES)
  set(NCBI_COMPONENT_Boost.Thread_INCLUDE ${NCBI_COMPONENT_Boost_INCLUDE})
#  set(NCBI_COMPONENT_Boost.Thread_LIBS    ${Boost_LIBRARIES})
else()
  set(NCBI_COMPONENT_Boost.Thread_FOUND NO)
endif()

#############################################################################
# JPEG
NCBI_define_Pkgcomponent(NAME JPEG PACKAGE libjpeg)

#############################################################################
# PNG
NCBI_define_Pkgcomponent(NAME PNG PACKAGE libpng)

#############################################################################
# GIF
NCBI_define_Pkgcomponent(NAME GIF PACKAGE giflib)

#############################################################################
# TIFF
NCBI_define_Pkgcomponent(NAME TIFF PACKAGE libtiff)

#############################################################################
# SQLITE3
NCBI_define_Pkgcomponent(NAME SQLITE3 PACKAGE sqlite3)
if(NCBI_COMPONENT_SQLITE3_FOUND)
    check_symbol_exists(sqlite3_unlock_notify ${NCBI_COMPONENT_SQLITE3_INCLUDE}/sqlite3.h HAVE_SQLITE3_UNLOCK_NOTIFY)
    check_include_file(sqlite3async.h HAVE_SQLITE3ASYNC_H -I${NCBI_COMPONENT_SQLITE3_INCLUDE})
endif()

#############################################################################
# BerkeleyDB
NCBI_define_Pkgcomponent(NAME BerkeleyDB PACKAGE libdb)
if(NCBI_COMPONENT_BerkeleyDB_FOUND)
    set(HAVE_BERKELEY_DB 1)
    set(HAVE_BDB         1)
    set(HAVE_BDB_CACHE   1)
endif()

#############################################################################
# XML
NCBI_define_Pkgcomponent(NAME XML PACKAGE libxml2)

#############################################################################
# XSLT
NCBI_define_Pkgcomponent(NAME XSLT PACKAGE libxslt)

#############################################################################
# EXSLT
NCBI_define_Pkgcomponent(NAME EXSLT PACKAGE libxslt)

#############################################################################
# UV
NCBI_define_Pkgcomponent(NAME UV PACKAGE libuv)

#############################################################################
# NGHTTP2
NCBI_define_Pkgcomponent(NAME NGHTTP2 PACKAGE libnghttp2)

##############################################################################
# GRPC/PROTOBUF
NCBI_define_Pkgcomponent(NAME PROTOBUF PACKAGE protobuf)
