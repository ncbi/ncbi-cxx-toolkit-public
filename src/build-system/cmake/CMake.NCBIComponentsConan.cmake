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

set(NCBI_REQUIRE_unix_FOUND YES)
list(APPEND NCBI_ALL_REQUIRES unix)

#set(NCBI_ALL_COMPONENTS "")

message("USING CONAN")

set(NCBI_PlatformBits 64)
set(CMAKE_CXX_STANDARD 14)

if(NOT APPLE AND NOT CYGWIN)
    set(NCBI_REQUIRE_Linux_FOUND YES)
    list(APPEND NCBI_ALL_REQUIRES Linux)
endif()

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

if(NOT APPLE)
    NCBI_define_Xcomponent(NAME GIF PACKAGE GIF LIB gif)
endif()

if (APPLE)
    find_library(NETWORK_LIBS resolv)
    find_library(RT_LIBS c)
elseif (NCBI_REQUIRE_FreeBSD_FOUND)
    find_library(NETWORK_LIBS c)
    find_library(RT_LIBS      rt)
else ()
    find_library(NETWORK_LIBS   resolv)
    find_library(RT_LIBS        rt)
endif ()
set(ORIG_LIBS   ${DL_LIBS} ${RT_LIBS} ${MATH_LIBS} ${CMAKE_THREAD_LIBS_INIT})

function(fetch_build_package packageurl)
  set(BASEDIR "/tmp/xxx.cmake.0123")
  execute_process(COMMAND rm -rf ${BASEDIR})
  file(MAKE_DIRECTORY ${BASEDIR})

  execute_process(COMMAND 
    git clone "${packageurl}" "package"
    WORKING_DIRECTORY "${BASEDIR}"
    RESULT_VARIABLE CLONE_RESULT
  )

  if(NOT CLONE_RESULT EQUAL "0")
    message(FATAL_ERROR "Can't git clone ${packageurl}")
  endif()

  execute_process(COMMAND
    conan install "."
    WORKING_DIRECTORY "${BASEDIR}/package"
    RESULT_VARIABLE CONAN_INSTALL_RESULT
  )

  if(NOT CONAN_INSTALL_RESULT EQUAL "0")
    message(FATAL_ERROR "Conan install error")
  endif()

  execute_process(COMMAND conan source . 
    WORKING_DIRECTORY "${BASEDIR}/package"
    RESULT_VARIABLE CONAN_SOURCE_RESULT
  )

  if(NOT CONAN_SOURCE_RESULT EQUAL "0")
    message(FATAL_ERROR "Conan source error")
  endif()

  execute_process(COMMAND conan export-pkg . -f
    WORKING_DIRECTORY "${BASEDIR}/package"
    RESULT_VARIABLE CONAN_EXPORT_RESULT
  )

  if(NOT CONAN_EXPORT_RESULT EQUAL "0")
    message(FATAL_ERROR "Conan export error")
  endif()
endfunction()


#not pip-installing conan for now
#ignoring compile flags
function(install_conan_package component pkgname)
  message("INSTALL_CONAN_PACKAGE ${component} ${pkgname}")
  set(BASEDIR "/tmp/xxx.cmake.0123.${pkgname}")
  file(REMOVE_RECURSE BASEDIR)
  file(MAKE_DIRECTORY BASEDIR)

  file(WRITE "${BASEDIR}/conanfile.txt" "[requires]\n${pkgname}\n\n[generators]\ncmake\n")

  execute_process(COMMAND 
    conan install "." "--build=missing" 
    WORKING_DIRECTORY "${BASEDIR}"
    RESULT_VARIABLE CONAN_INSTALL_RESULT
  )

  if(NOT CONAN_INSTALL_RESULT EQUAL "0")
    message(FATAL_ERROR "Conan install error")
  endif()
  file(STRINGS "${BASEDIR}/conanbuildinfo.txt" DEPVARS)
  set(PARSER "normal")
  set(INCLUDES "")
  set(LIBDIRS "")
  set(LIBS "")
  set(DEFINES "")
  set(BUILDDIRS "")
  foreach(LN ${DEPVARS})
    string(STRIP "${LN}" LNS)
    string(REGEX MATCH "\\[.*\\]" _cmd  ${LNS})
    if(_cmd)
      set(PARSER "normal")
    endif()
    if(${PARSER} STREQUAL "normal")
      if(${LNS} STREQUAL "[includedirs]")
        set(PARSER "includes")
      elseif(${LNS} STREQUAL "[libdirs]")
        set(PARSER "libdirs")
      elseif(${LNS} STREQUAL "[libs]")
        set(PARSER "libs")
      elseif(${LNS} STREQUAL "[defines]")
        set(PARSER "defs")
      elseif(${LNS} STREQUAL "[builddirs]")
        set(PARSER "builddirs")
      endif()
    elseif(${PARSER} STREQUAL "includes")
      if(${LNS} STREQUAL "")
        continue()
      endif()
      list(APPEND INCLUDES "${LNS}")
    elseif(${PARSER} STREQUAL "defs")
      if(${LNS} STREQUAL "")
        continue()
      endif()
      list(APPEND DEFINES "-D${LNS}")
    elseif(${PARSER} STREQUAL "libdirs")
      if(${LNS} STREQUAL "")
        continue()
      endif()
      list(APPEND LIBDIRS "-L${LNS}")
    elseif(${PARSER} STREQUAL "libs")
      if(${LNS} STREQUAL "")
        continue()
      endif()
      list(APPEND LIBS "-l${LNS}")
    elseif(${PARSER} STREQUAL "builddirs")
      if(${LNS} STREQUAL "")
        continue()
      endif()
      message("XXX PROTOBUF APPEND ${LNS}")
      list(APPEND BUILDDIRS "${LNS}")
    endif()
  endforeach()
  message("INCLUDES ${INCLUDES}")
  message("LIBDIRS ${LIBDIRS}")
  message("LIBS ${LIBS}")

  set(NCBI_COMPONENT_${component}_FOUND YES PARENT_SCOPE)
  set(NCBI_LIB${component} 1 PARENT_SCOPE)
  set(NCBI_${component} 1 PARENT_SCOPE)
  set(NCBI_COMPONENT_${component}_DEFINES ${DEFINES} PARENT_SCOPE)
  set(NCBI_COMPONENT_${component}_INCLUDE ${INCLUDES} PARENT_SCOPE)
  set(NCBI_COMPONENT_${component}_LIBS ${LIBDIRS} ${LIBS} PARENT_SCOPE)
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} ${component}")
  set(NCBI_ALL_COMPONENTS ${NCBI_ALL_COMPONENTS} PARENT_SCOPE)
  set(HAVE_LIB${component} 1 PARENT_SCOPE)
  set(HAVE_${component} 1 PARENT_SCOPE)

  if(${component} STREQUAL "PROTOBUF")
    list(GET BUILDDIRS 0 PBDIR)
    set(NCBI_PROTOC_APP "${PBDIR}/bin/protoc")
    set(NCBI_PROTOC_APP "${PBDIR}/bin/protoc" PARENT_SCOPE)
    message("XXX PROTOC ${PBDIR} // ${NCBI_PROTOC_APP} //// ${BUILDDIRS}")
  endif()
endfunction()

# Load conan package
# grpc/1.31.1@inexorgame/stable
# grpc/1.28.1@inexorgame/stable
install_conan_package("Z" "zlib/1.2.11@conan/stable")
install_conan_package("TIFF" "libtiff/4.0.9@bincrafters/stable")
#Wild giflib is not compatible with toolkit
#install_conan_package("GIF" "giflib/5.1.4@bincrafters/stable")
install_conan_package("PNG" "libpng/1.6.37@bincrafters/stable")
install_conan_package("JPEG" "libjpeg/9c@bincrafters/stable")
if(NOT APPLE)
	install_conan_package("UNWIND" "libunwind/1.3.1@bincrafters/stable")
endif()
install_conan_package("PCRE" "pcre/8.41@bincrafters/stable")
install_conan_package("NETTLE" "nettle/3.4.1@bincrafters/stable")
install_conan_package("UV" "libuv/1.31.0@bincrafters/stable")
install_conan_package("LIBUV" "libuv/1.31.0@bincrafters/stable")
install_conan_package("TLS" "mbedtls/2.23.0")
install_conan_package("NGHTTP2" "libnghttp2/1.39.2")
install_conan_package("BZ2" "bzip2/1.0.8@conan/stable")
install_conan_package("BerkeleyDB" "libdb/5.3.28")
install_conan_package("Boost.Test.Included" "boost/1.69.0@conan/stable")
install_conan_package("Boost.Spirit" "boost/1.69.0@conan/stable")
install_conan_package("XML" "libxml2/2.9.9@bincrafters/stable")
install_conan_package("XSLT" "libxslt/1.1.33@bincrafters/stable")
#install_conan_package("EXSLT" "libxslt/1.1.33@bincrafters/stable")
#install_conan_package("PROTOBUF" "protobuf/3.6.1@bincrafters/stable")
#install_conan_package("PROTOBUF" "protobuf/3.12.4")
#install_conan_package("wxWidgets" "wxwidgets/3.1.3@bincrafters/stable")

# GRPC is not orking yet
#fetch_build_package("https://github.com/inexorgame/conan-grpc.git")
#install_conan_package("GRPC" "grpc/0.0.1")

install_conan_package("SQLITE3" "sqlite3/3.29.0@bincrafters/stable")
if(NCBI_COMPONENT_SQLITE3_FOUND)
    check_symbol_exists(sqlite3_unlock_notify ${NCBI_COMPONENT_SQLITE3_INCLUDE}/sqlite3.h HAVE_SQLITE3_UNLOCK_NOTIFY)
    check_include_file(sqlite3async.h HAVE_SQLITE3ASYNC_H -I${NCBI_COMPONENT_SQLITE3_INCLUDE})
endif()

