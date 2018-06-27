#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper
##
##
##---------------------------------------------------------------------------
##  in CMakeLists.txt:
##    NCBI_add_subdirectory( list of subdirectories)
##    NCBI_add_library(      list of libraries)
##    NCBI_add_app(          list of apps)
##---------------------------------------------------------------------------
##  the following calls in CMakeLists.txt affect all projects in current dir and all subdirs
##    NCBI_headers( list of headers)
##    NCBI_disable_pch() / NCBI_enable_pch()
##    NCBI_requires( list of components)
##    NCBI_optional_components(  list of components)
##    NCBI_project_tags(list of tags)
##    
##---------------------------------------------------------------------------
##  in CMakeLists.xxx.[lib|app].txt - all calls affect current project only
##    NCBI_begin_lib(name) or NCBI_begin_app(name)
##
##      NCBI_sources(   list of source files)
##      NCBI_headers(   list of header files) - only relative paths and masks are allowed
##      NCBI_resources( list of resource files) - file extension is mandatory
##      NCBI_dataspecs( list of data specs - ASN, DTD, XSD etc) - file extension is mandatory
##
##      NCBI_requires( list of components)
##      NCBI_optional_components(  list of components)
##
##      NCBI_enable_pch() / NCBI_disable_pch() - no arguments
##      NCBI_disable_pch_for( list of source files)
##      NCBI_set_pch_header( header file)
##      NCBI_set_pch_define( macro to define)
##
##      NCBI_uses_toolkit_libraries(  list of libraries)
##      NCBI_uses_external_libraries( list of libraries)
##      NCBI_add_definitions(         list of compiler definitions)
##      NCBI_add_include_directories( list of directories)
##
##      NCBI_project_tags(     list of tags)
##      NCBI_project_watchers( list of watchers)
##
##      NCBI_add_test( test command) - empty command means run the app with no arguments
##    NCBI_end_lib(result) or NCBI_end_app(result) - argument 'result' is optional
##

#############################################################################
function(NCBI_add_root_subdirectory)

  set(NCBI_PTBMODE_COLLECT_DEPS OFF)
  if (NCBI_EXPERIMENTAL_ENABLE_COLLECTOR)
    set(NCBI_PTBMODE_COLLECT_DEPS ON)
    set(NCBI_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    set(_curdir ${NCBI_CURRENT_SOURCE_DIR})
    set_property(GLOBAL PROPERTY NCBI_PTB_PROJECTS "")

    foreach(_sub IN LISTS ARGV)
      if (EXISTS "${_curdir}/${_sub}/CMakeLists.txt")
        set(NCBI_CURRENT_SOURCE_DIR ${_curdir}/${_sub})
        include("${_curdir}/${_sub}/CMakeLists.txt")
      else()
        message(WARNING "ERROR: directory not found: ${_curdir}/${_sub}")
      endif()
    endforeach()

    if(NCBI_EXPERIMENTAL_VERBOSE)
      get_property(_deps GLOBAL PROPERTY NCBI_PTB_PROJECTS)
      message("NCBI_PTB_PROJECTS: ${_deps}")
      foreach(_prj IN LISTS _deps)
        get_property(_prjdeps GLOBAL PROPERTY NCBI_PTBDEPS_${_prj})
        message("NCBI_PTBDEPS_${_prj}: ${_prjdeps}")
      endforeach()
    endif()

    foreach(_prj IN LISTS NCBI_EXPERIMENTAL_PROJECTS)
      NCBI_internal_collect_dependencies(${_prj})
      get_property(_prjdeps GLOBAL PROPERTY NCBI_PTBDEPS_${_prj})
      set(NCBI_PTB_ALLOWED_PROJECTS ${NCBI_PTB_ALLOWED_PROJECTS} ${_prj} ${_prjdeps})
    endforeach()
    list(SORT NCBI_PTB_ALLOWED_PROJECTS)
    list(REMOVE_DUPLICATES NCBI_PTB_ALLOWED_PROJECTS)
    if(NCBI_EXPERIMENTAL_VERBOSE)
      message("NCBI_PTB_ALLOWED_PROJECTS: ${NCBI_PTB_ALLOWED_PROJECTS}")
    endif()
    set(NCBI_PTBMODE_COLLECT_DEPS OFF)
  endif()

  foreach(_sub IN LISTS ARGV)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_sub}")
      add_subdirectory(${_sub})
    else()
      message(WARNING "ERROR: directory not found: ${CMAKE_CURRENT_SOURCE_DIR}/${_sub}")
    endif()
  endforeach()
endfunction()

#############################################################################
function(NCBI_add_subdirectory)
  if(NCBI_PTBMODE_COLLECT_DEPS)
    set(_curdir ${NCBI_CURRENT_SOURCE_DIR})
    foreach(_sub IN LISTS ARGV)
      if (EXISTS "${_curdir}/${_sub}/CMakeLists.txt")
        set(NCBI_CURRENT_SOURCE_DIR ${_curdir}/${_sub})
        include("${_curdir}/${_sub}/CMakeLists.txt")
      else()
        message(WARNING "ERROR: directory not found: ${_curdir}/${_sub}")
      endif()
    endforeach()
  else()
    foreach(_sub IN LISTS ARGV)
      set(NCBI_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${_sub})
      if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_sub}")
        add_subdirectory(${_sub})
      else()
        message(WARNING "ERROR: directory not found: ${CMAKE_CURRENT_SOURCE_DIR}/${_sub}")
      endif()
    endforeach()
  endif()
endfunction()

#############################################################################
function(NCBI_add_library)
  if(NCBI_PTBMODE_COLLECT_DEPS)
    foreach(_lib IN LISTS ARGV)
      if (EXISTS ${NCBI_CURRENT_SOURCE_DIR}/CMakeLists.${_lib}.lib.txt)
        NCBI_internal_include_project(${NCBI_CURRENT_SOURCE_DIR}/CMakeLists.${_lib}.lib.txt)
      elseif (EXISTS ${NCBI_CURRENT_SOURCE_DIR}/CMakeLists.${_lib}.asn.txt)
        NCBI_internal_include_project(${NCBI_CURRENT_SOURCE_DIR}/CMakeLists.${_lib}.asn.txt)
      else()
        message(WARNING "ERROR: file not found: ${NCBI_CURRENT_SOURCE_DIR}/CMakeLists.${_lib}.lib.txt")
      endif()
    endforeach()
  else()
    foreach(_lib IN LISTS ARGV)
      if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.${_lib}.lib.txt)
        NCBI_internal_include_project(CMakeLists.${_lib}.lib.txt)
      elseif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.${_lib}.asn.txt)
        NCBI_internal_include_project(CMakeLists.${_lib}.asn.txt)
      else()
        message(WARNING "ERROR: file not found: ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.${_lib}.lib.txt")
      endif()
    endforeach()
  endif()
endfunction()

#############################################################################
function(NCBI_add_app)
  if(NCBI_PTBMODE_COLLECT_DEPS)
    foreach(_app IN LISTS ARGV)
      if (EXISTS ${NCBI_CURRENT_SOURCE_DIR}/CMakeLists.${_app}.app.txt)
        NCBI_internal_include_project(${NCBI_CURRENT_SOURCE_DIR}/CMakeLists.${_app}.app.txt)
      else()
        message(WARNING "ERROR: file not found: ${NCBI_CURRENT_SOURCE_DIR}/CMakeLists.${_app}.app.txt")
      endif()
    endforeach()
  else()
    foreach(_app IN LISTS ARGV)
      if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.${_app}.app.txt)
        NCBI_internal_include_project(CMakeLists.${_app}.app.txt)
      else()
        message(WARNING "ERROR: file not found: ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.${_app}.app.txt")
      endif()
    endforeach()
  endif()
endfunction()

#############################################################################
macro(NCBI_begin_lib _name)
  set(NCBI_PROJECT_lib ${_name})
  if(NCBI_EXPERIMENTAL_CFG)
    set(NCBI_PROJECT ${_name})
    set(NCBI_${NCBI_PROJECT}_OUTPUT ${_name})
  else()
    if("${_name}" STREQUAL "general")
      set(NCBI_PROJECT ${_name}-lib)
      set(NCBI_${NCBI_PROJECT}_OUTPUT ${_name})
    else()
      set(NCBI_PROJECT ${_name})
      set(NCBI_${NCBI_PROJECT}_OUTPUT ${_name})
    endif()
  endif()
#CMake global flag
  if(BUILD_SHARED_LIBS)
    set(NCBI_${NCBI_PROJECT}_TYPE SHARED)
  else()
    set(NCBI_${NCBI_PROJECT}_TYPE STATIC)
  endif()
endmacro()

#############################################################################
macro(NCBI_end_lib)
  if(NOT DEFINED NCBI_PROJECT_lib)
    message(SEND_ERROR "${NCBI_CURRENT_SOURCE_DIR}/${NCBI_PROJECT}: Unexpected NCBI_end_lib call")
  endif()
  NCBI_internal_add_project()
  unset(NCBI_PROJECT)
endmacro()

#############################################################################
macro(NCBI_begin_app _name)
  set(NCBI_PROJECT_app ${_name})
  if(NCBI_EXPERIMENTAL_CFG)
    set(NCBI_PROJECT ${_name})
    set(NCBI_${NCBI_PROJECT}_OUTPUT ${_name})
  else()
    set(NCBI_PROJECT ${_name}-app)
    set(NCBI_${NCBI_PROJECT}_OUTPUT ${_name})
  endif()
  set(NCBI_${NCBI_PROJECT}_TYPE CONSOLEAPP)
endmacro()

#############################################################################
macro(NCBI_end_app)
  if(NOT DEFINED NCBI_PROJECT_app)
    message(SEND_ERROR "${NCBI_CURRENT_SOURCE_DIR}/${NCBI_PROJECT}: Unexpected NCBI_end_app call")
  endif()
  NCBI_internal_add_project()
  unset(NCBI_PROJECT)
endmacro()

#############################################################################
macro(NCBI_sources)
  set(NCBI_${NCBI_PROJECT}_SOURCES ${NCBI_${NCBI_PROJECT}_SOURCES} "${ARGV}")
endmacro()

#############################################################################
macro(NCBI_headers)
  set(NCBI_${NCBI_PROJECT}_HEADERS ${NCBI_${NCBI_PROJECT}_HEADERS} "${ARGV}")
endmacro()

#############################################################################
macro(NCBI_resources)
  set(NCBI_${NCBI_PROJECT}_RESOURCES ${NCBI_${NCBI_PROJECT}_RESOURCES} "${ARGV}")
endmacro()

#############################################################################
macro(NCBI_dataspecs)
  set(NCBI_${NCBI_PROJECT}_DATASPEC ${NCBI_${NCBI_PROJECT}_DATASPEC} "${ARGV}")
endmacro()

#############################################################################
macro(NCBI_requires)
  set(NCBI_${NCBI_PROJECT}_REQUIRES ${NCBI_${NCBI_PROJECT}_REQUIRES} "${ARGV}")
endmacro()

#############################################################################
macro(NCBI_optional_components)
  set(NCBI_${NCBI_PROJECT}_COMPONENTS ${NCBI_${NCBI_PROJECT}_COMPONENTS} "${ARGV}")
endmacro()

#############################################################################
macro(NCBI_enable_pch)
  set(NCBI_${NCBI_PROJECT}_USEPCH ON)
endmacro()
macro(NCBI_disable_pch)
  set(NCBI_${NCBI_PROJECT}_USEPCH OFF)
endmacro()

#############################################################################
macro(NCBI_disable_pch_for)
  set(NCBI_${NCBI_PROJECT}_NOPCH ${NCBI_${NCBI_PROJECT}_NOPCH} "${ARGV}")
endmacro()

#############################################################################
macro(NCBI_set_pch_header)
  set(NCBI_${NCBI_PROJECT}_PCH "${ARGV}")
endmacro()

#############################################################################
macro(NCBI_set_pch_define)
  set(NCBI_${NCBI_PROJECT}_PCH_DEFINE "${ARGV}")
endmacro()

#############################################################################
macro(NCBI_uses_toolkit_libraries)
  set(_tk_libs "${ARGV}")
  foreach(_tk_lib IN LISTS _tk_libs)
    if("${_tk_lib}" STREQUAL "general")
      if(NCBI_EXPERIMENTAL_CFG)
        set(NCBI_${NCBI_PROJECT}_NCBILIB ${NCBI_${NCBI_PROJECT}_NCBILIB} \$<1:general>)
      else()
        set(NCBI_${NCBI_PROJECT}_NCBILIB ${NCBI_${NCBI_PROJECT}_NCBILIB} general-lib)
      endif()
    else()
      set(NCBI_${NCBI_PROJECT}_NCBILIB ${NCBI_${NCBI_PROJECT}_NCBILIB} ${_tk_lib})
    endif()
  endforeach()
endmacro()

#############################################################################
macro(NCBI_uses_external_libraries)
  set(NCBI_${NCBI_PROJECT}_EXTLIB ${NCBI_${NCBI_PROJECT}_EXTLIB} "${ARGV}")
endmacro()

#############################################################################
macro(NCBI_add_include_directories)
  set(NCBI_${NCBI_PROJECT}_INCLUDES ${NCBI_${NCBI_PROJECT}_INCLUDES} "${ARGV}")
endmacro()

#############################################################################
macro(NCBI_add_definitions)
  set(NCBI_${NCBI_PROJECT}_DEFINES ${NCBI_${NCBI_PROJECT}_DEFINES} "${ARGV}")
endmacro()

#############################################################################
macro(NCBI_project_tags)
  set(NCBI_${NCBI_PROJECT}_PROJTAG ${NCBI_${NCBI_PROJECT}_PROJTAG} "${ARGV}")
endmacro()

#############################################################################
macro(NCBI_project_watchers)
  set(NCBI_${NCBI_PROJECT}_WATCHER ${NCBI_${NCBI_PROJECT}_WATCHER} "${ARGV}")
endmacro()

#############################################################################
#############################################################################
#############################################################################
macro(NCBI_begin_test _name)
  set(NCBI_TEST "${_name}")
endmacro()

##############################################################################
macro(NCBI_set_test_command)
if (ON)
  set( _args ${ARGV})
  list(GET _args 0 _cmd)
  list(REMOVE_AT _args 0) 
  if ( "${_cmd}" STREQUAL "${NCBI_PROJECT}")
    set(_cmd ${NCBI_${NCBI_PROJECT}_OUTPUT})
  endif()
  if (NOT NCBI_EXPERIMENTAL_CFG)
    set(_cmd ${EXECUTABLE_OUTPUT_PATH}/${_cmd})
  endif()
  set(NCBI_${NCBI_TEST}_CMD "${_cmd}")
  set(NCBI_${NCBI_TEST}_ARG "${_args}")
  set(NCBI_${NCBI_TEST}_DIR "${NCBI_CURRENT_SOURCE_DIR}")
else()
  set(NCBI_${NCBI_TEST}_CMD "${ARGV}")
endif()
endmacro()

##############################################################################
macro(NCBI_set_test_arguments)
  if( NOT DEFINED NCBI_${NCBI_TEST}_CMD)
    set(NCBI_${NCBI_TEST}_CMD ${NCBI_PROJECT})
  endif()
  set(NCBI_${NCBI_TEST}_ARG "${ARGV}")
  set(NCBI_${NCBI_TEST}_DIR "${NCBI_CURRENT_SOURCE_DIR}")
endmacro()

##############################################################################
macro(NCBI_end_test)
  set(NCBI_ALLTESTS ${NCBI_ALLTESTS} ${NCBI_TEST})
  unset(NCBI_TEST)
endmacro()

##############################################################################
macro(NCBI_add_test)
  if (DEFINED NCBI_${NCBI_PROJECT}_TESTNUM)
    math(EXPR NCBI_${NCBI_PROJECT}_TESTNUM "${NCBI_${NCBI_PROJECT}_TESTNUM} + 1")
  endif()

  if ("${ARGC}" GREATER "0")
    set(_cmd "${ARGV}")
    if ( "${_cmd}" STREQUAL "")
      set(_cmd ${NCBI_${NCBI_PROJECT}_OUTPUT})
    endif()
  else()
    set(_cmd ${NCBI_${NCBI_PROJECT}_OUTPUT})
  endif()

  NCBI_begin_test(${NCBI_PROJECT}${NCBI_${NCBI_PROJECT}_TESTNUM})
  NCBI_set_test_command(${_cmd})
  NCBI_end_test()

  if (NOT DEFINED NCBI_${NCBI_PROJECT}_TESTNUM)
    set(NCBI_${NCBI_PROJECT}_TESTNUM "1")
  endif()
endmacro()


##############################################################################
##############################################################################
##############################################################################

function(NCBI_internal_collect_dependencies _project)
  get_property(_prjdeps GLOBAL PROPERTY NCBI_PTBDEPS_${_project} SET)
  if (NOT _prjdeps)
    message(WARNING "ERROR: project ${_project} not found")
    return()
  endif()
  get_property(_prjdeps GLOBAL PROPERTY NCBI_PTBDEPS_${_project})
  foreach( _value IN LISTS _prjdeps)
    NCBI_internal_recur_collect_dependencies( ${_project} ${_value})
  endforeach()
endfunction()

##############################################################################
function(NCBI_internal_recur_collect_dependencies _project _dep)
  get_property(_prjdeps GLOBAL PROPERTY NCBI_PTBDEPS_${_project})
  get_property(_depdeps GLOBAL PROPERTY NCBI_PTBDEPS_${_dep})
  set_property(GLOBAL PROPERTY NCBI_PTBDEPS_${_project} ${_prjdeps} ${_depdeps})
  foreach( _value IN LISTS _depdeps)
    list(FIND _prjdeps ${_value} _found)
    if (${_found} LESS "0")
      NCBI_internal_recur_collect_dependencies( ${_project} ${_value})
    endif()
  endforeach()
endfunction()

##############################################################################
function(NCBI_internal_include_project)
  include(${ARGV0})
endfunction()

##############################################################################
function(NCBI_internal_collect_sources)
  set(_dir "${ARGV0}")
  if(NCBI_PTBMODE_DLL)
    set(_prefix "Hosted Libraries\\${ARGV1}\\")
  else()
    set(_prefix "")
  endif()

  foreach(_file IN LISTS NCBI_${NCBI_PROJECT}_SOURCES)
    if(EXISTS ${_dir}/${_file}.cpp)
      list(APPEND _sources "${_file}.cpp")
    elseif(EXISTS ${_dir}/${_file}.c)
      list(APPEND _sources "${_file}.c")
      set(_nopch ${_nopch} "${_file}.c")
    else()
      list(APPEND _sources "${_file}")
      get_filename_component(_ext ${_file} EXT)
      if("${_ext}" STREQUAL ".c")
        set(_nopch ${_nopch} ${_file})
      endif()
    endif()
  endforeach()
  source_group("${_prefix}Source Files"    FILES ${_pub})
  set(NCBITMP_PROJECT_SOURCES ${_sources} PARENT_SCOPE)
  set(NCBI_${NCBI_PROJECT}_NOPCH ${NCBI_${NCBI_PROJECT}_NOPCH} ${_nopch} PARENT_SCOPE)
endfunction()

##############################################################################
function(NCBI_internal_collect_headers)

  file(RELATIVE_PATH _rel ${NCBI_SRC_ROOT} ${ARGV0})
  set(_inc_dir ${NCBI_INC_ROOT}/${_rel})
  if(NCBI_PTBMODE_DLL)
    set(_prefix "Hosted Libraries\\${ARGV1}\\")
  else()
    set(_prefix "")
  endif()
  if (DEFINED NCBI_${NCBI_PROJECT}_HEADERS)
    set(_headers ${NCBI_${NCBI_PROJECT}_HEADERS})
  elseif(DEFINED NCBI__HEADERS)
    set(_headers ${NCBI__HEADERS})
  else()
    set(_headers *.h* *impl/*.h* *.inl *impl/*.inl)
  endif()

  if (MSVC OR XCODE)
    set(_priv "")
    set(_pub "")
    set(_inl "")
    if ( NOT "${_headers}" STREQUAL "")
      foreach(_mask ${_headers})
        file(GLOB _files LIST_DIRECTORIES false "${ARGV0}/${_mask}")
        set(_priv ${_priv} ${_files})
        if (${_mask} MATCHES ".inl$")
          file(GLOB _files LIST_DIRECTORIES false "${_inc_dir}/${_mask}")
          set(_inl ${_inl} ${_files})
        else()
          file(GLOB _files LIST_DIRECTORIES false "${_inc_dir}/${_mask}")
          set(_pub ${_pub} ${_files})
        endif()
      endforeach()

      if ( NOT "${_priv}" STREQUAL "")
        source_group("${_prefix}Private Headers" FILES ${_priv})
      endif()
      if ( NOT "${_pub}" STREQUAL "")
        source_group("${_prefix}Header Files"    FILES ${_pub})
      endif()
      if ( NOT "${_inl}" STREQUAL "")
        source_group("${_prefix}Inline Files"    FILES ${_inl})
      endif()
    endif()
    set(NCBITMP_PROJECT_HEADERS ${_priv} ${_pub} ${_inl} PARENT_SCOPE)
  else()
    set(NCBITMP_PROJECT_HEADERS "" PARENT_SCOPE)
  endif()
endfunction()

##############################################################################
function(NCBI_internal_add_resources) 
  if (MSVC)
    if ("${NCBI_${NCBI_PROJECT}_TYPE}" STREQUAL "CONSOLEAPP")
      if (DEFINED NCBI_${NCBI_PROJECT}_RESOURCES)
        set(_res ${NCBI_${NCBI_PROJECT}_RESOURCES})
      elseif (DEFINED NCBI__RESOURCES)
        set(_res ${NCBI__RESOURCES})
      else()
        set(_res ${NCBI_DEFAULT_RESOURCES})
      endif()
    else()
      if (DEFINED NCBI_${NCBI_PROJECT}_RESOURCES)
        set(_res ${NCBI_${NCBI_PROJECT}_RESOURCES})
      endif()
    endif()

    if(NCBI_PTBMODE_DLL)
      set(_prefix "Hosted Libraries\\${ARGV0}\\")
    else()
      set(_prefix "")
    endif()
    if ( NOT "${_res}" STREQUAL "")
      source_group("${_prefix}Resource Files" FILES ${_res})
    endif()
    set(NCBITMP_PROJECT_RESOURCES ${_res} PARENT_SCOPE)
  else()
    set(NCBITMP_PROJECT_RESOURCES "" PARENT_SCOPE)
  endif()
endfunction()

##############################################################################
function(NCBI_internal_add_dataspec) 

  if (DEFINED NCBI_${NCBI_PROJECT}_DATASPEC)

    foreach(_dataspec ${NCBI_${NCBI_PROJECT}_DATASPEC})

      get_filename_component(_basename ${_dataspec} NAME_WE)
      get_filename_component(_ext ${_dataspec} EXT)
      set(_filepath ${CMAKE_CURRENT_SOURCE_DIR}/${_dataspec})
      get_filename_component(_path ${_filepath} DIRECTORY)
      file(RELATIVE_PATH _relpath ${NCBI_SRC_ROOT} ${_path})
      set(_module_imports "")
      set(_imports "")

      if(EXISTS "${_path}/${_basename}.module")
        FILE(READ "${_path}/${_basename}.module" _module_contents)
        STRING(REGEX MATCH "MODULE_IMPORT *=[^\n]*[^ \n]" _tmp "${_module_contents}")
        STRING(REGEX REPLACE "MODULE_IMPORT *= *" "" _tmp "${_tmp}")
        STRING(REGEX REPLACE "  *$" "" _imp_list "${_tmp}")
        STRING(REGEX REPLACE " " ";" _imp_list "${_imp_list}")

        foreach(_module IN LISTS _imp_list)
            set(_module_imports "${_module_imports} ${_module}${_ext}")
        endforeach()
        if (NOT "${_module_imports}" STREQUAL "")
          set(_imports -M ${_module_imports})
        endif()
      endif()

      set_source_files_properties(${_path}/${_basename}__.cpp ${_path}/${_basename}___.cpp PROPERTIES GENERATED TRUE)
      set(_srcfiles ${_srcfiles} ${_path}/${_basename}__.cpp ${_path}/${_basename}___.cpp)

      set(_oc ${_basename})
      if (NOT "${NCBI_DEFAULT_PCH}" STREQUAL "")
        set(_pch -pch ${NCBI_DEFAULT_PCH})
      endif()
      set(_od ${_path}/${_basename}.def)
      set(_oex -oex " ")
      set(_cmd ${NCBI_DATATOOL} ${_oex} ${_pch} -m ${_filepath} -oA -oc ${_oc} -od ${_od} -odi -ocvs -or ${_relpath} -oR ${top_src_dir} ${_imports})
      add_custom_command(
            OUTPUT ${_path}/${_basename}__.cpp ${_path}/${_basename}___.cpp
            COMMAND ${_cmd} VERBATIM
            WORKING_DIRECTORY ${top_src_dir}
            COMMENT "Generate C++ classes from ${_filepath}"
            DEPENDS ${NCBI_DATATOOL}
            VERBATIM
        )
    endforeach()
    set(NCBITMP_PROJECT_SOURCES ${NCBITMP_PROJECT_SOURCES} ${_srcfiles} PARENT_SCOPE)
    if (MSVC OR XCODE)
      if(NCBI_PTBMODE_DLL)
        set(_prefix "Hosted Libraries\\${ARGV0}\\")
      else()
        set(_prefix "")
      endif()

      source_group("${_prefix}DataSpec Files" FILES ${NCBI_${NCBI_PROJECT}_DATASPEC})
    endif()

  endif()
endfunction()

##############################################################################
function(NCBI_internal_define_precompiled_header_usage)

  if (DEFINED NCBI_${NCBI_PROJECT}_USEPCH)
    set(_usepch ${NCBI_${NCBI_PROJECT}_USEPCH})
  elseif (DEFINED NCBI__USEPCH)
    set(_usepch ${NCBI__USEPCH})
  else()
    set(_usepch ${NCBI_DEFAULT_USEPCH})
  endif()

  if (DEFINED NCBI_${NCBI_PROJECT}_PCH)
    set(_pch ${NCBI_${NCBI_PROJECT}_PCH})
  elseif (DEFINED NCBI__PCH)
    set(_pch ${NCBI__PCH})
  else()
    set(_pch ${NCBI_DEFAULT_PCH})
  endif()

  if (DEFINED NCBI_${NCBI_PROJECT}_PCH_DEFINE)
    set(_pchdef ${NCBI_${NCBI_PROJECT}_PCH_DEFINE})
  elseif (DEFINED NCBI__PCH_DEFINE)
    set(_pchdef ${NCBI__PCH_DEFINE})
  else()
    set(_pchdef ${NCBI_DEFAULT_PCH_DEFINE})
  endif()

if(NCBI_EXPERIMENTAL_VERBOSE OR NCBI_EXPERIMENTAL_VERBOSE_${NCBI_PROJECT})
  message("_usepch ${_usepch}")
  message("_pch ${_pch}")
  message("_pchdef ${_pchdef}")
endif()
  if (NOT DEFINED NCBITMP_PROJECT_SOURCES)
    set(_usepch OFF)
message("--------no sources")
  endif()

  if (MSVC)
    if (_usepch)
      set_target_properties(${NCBI_PROJECT} PROPERTIES COMPILE_FLAGS "/Yu${_pch}")
      set(_files ${NCBITMP_PROJECT_SOURCES})
      if (DEFINED NCBI_${NCBI_PROJECT}_NOPCH)
        foreach(_file ${NCBI_${NCBI_PROJECT}_NOPCH})
          list(FIND _files ${_file} _found)
          if (${_found} LESS "0")
            list(FIND _files ${_file}.cpp _found)
            if (${_found} LESS "0")
              list(FIND _files ${_file}.c _found)
              if (${_found} LESS "0")
                continue()
              endif()
              set(_file ${_file}.c)
            else()
              set(_file ${_file}.cpp)
            endif()
          endif()
          list(REMOVE_ITEM _files ${_file})
          set_source_files_properties(${_file} PROPERTIES COMPILE_FLAGS "/Y-")
        endforeach()
      endif()
      list(SORT _files)
      set(_first ON)    
      foreach(_file ${_files})
        if(_first)
          set_source_files_properties(${_file} PROPERTIES COMPILE_FLAGS "/Yc${_pch}")
          set(_first OFF)
#        else()
#          set_source_files_properties(${_file} PROPERTIES COMPILE_FLAGS "/Yu${_pch}")
        endif()
        set_source_files_properties(${_file} PROPERTIES COMPILE_DEFINITIONS ${_pchdef})
      endforeach()
    endif(_usepch)
  endif (MSVC)
endfunction()

##############################################################################
macro(NCBI_internal_process_project_requires)
  set(NCBITMP_REQUIRE_NOTFOUND "")

  foreach(_req IN LISTS NCBI__REQUIRES NCBI_${NCBI_PROJECT}_REQUIRES)
    string(SUBSTRING ${_req} 0 1 _sign)
    if ("${_sign}" STREQUAL "-")
      string(SUBSTRING ${_req} 0 1 _value)
      set(_negate ON)
    else()
      set(_value ${_req})
      set(_negate OFF)
    endif()
    if (NCBI_REQUIRE_${_value}_FOUND OR NCBI_COMPONENT_${_value}_FOUND)
      if (_negate)
        set(NCBITMP_REQUIRE_NOTFOUND ${NCBITMP_REQUIRE_NOTFOUND} ${_req})
      else()
        set(NCBITMP_INCLUDES ${NCBITMP_INCLUDES} ${NCBI_COMPONENT_${_value}_INCLUDE})
        set(NCBITMP_DEFINES  ${NCBITMP_DEFINES}  ${NCBI_COMPONENT_${_value}_DEFINES})
        set(NCBITMP_LIBS     ${NCBITMP_LIBS}     ${NCBI_COMPONENT_${_value}_LIBS})
        foreach(_lib IN LISTS NCBI_COMPONENT_${_value}_NCBILIB)
          if(NOT ${_lib} STREQUAL ${NCBI_${NCBI_PROJECT}_OUTPUT})
            set(NCBITMP_NCBILIB  ${NCBITMP_NCBILIB}  ${_lib})
          endif()
        endforeach()
      endif()
    else()
      if (_negate)
# no-no, this is a mistake
#        set(NCBITMP_INCLUDES ${NCBITMP_INCLUDES} ${NCBI_COMPONENT_${_value}_INCLUDE})
#        set(NCBITMP_DEFINES  ${NCBITMP_DEFINES}  ${NCBI_COMPONENT_${_value}_DEFINES})
#        set(NCBITMP_LIBS     ${NCBITMP_LIBS}     ${NCBI_COMPONENT_${_value}_LIBS})
      else()
        set(NCBITMP_REQUIRE_NOTFOUND ${NCBITMP_REQUIRE_NOTFOUND} ${_req})
      endif()
    endif()
  endforeach()
endmacro()

##############################################################################
macro(NCBI_internal_process_project_components)
  set(NCBITMP_COMPONENT_NOTFOUND "")

  foreach(_value IN LISTS NCBI__COMPONENTS NCBI_${NCBI_PROJECT}_COMPONENTS)
    if (NCBI_REQUIRE_${_value}_FOUND OR NCBI_COMPONENT_${_value}_FOUND)
      set(NCBITMP_INCLUDES ${NCBITMP_INCLUDES} ${NCBI_COMPONENT_${_value}_INCLUDE})
      set(NCBITMP_DEFINES  ${NCBITMP_DEFINES}  ${NCBI_COMPONENT_${_value}_DEFINES})
      set(NCBITMP_LIBS     ${NCBITMP_LIBS}     ${NCBI_COMPONENT_${_value}_LIBS})
        foreach(_lib IN LISTS NCBI_COMPONENT_${_value}_NCBILIB)
          if(NOT ${_lib} STREQUAL ${NCBI_${NCBI_PROJECT}_OUTPUT})
            set(NCBITMP_NCBILIB  ${NCBITMP_NCBILIB}  ${_lib})
          endif()
        endforeach()
    else()
      set(NCBITMP_COMPONENT_NOTFOUND ${NCBITMP_COMPONENT_NOTFOUND} ${_value})
    endif()
  endforeach()
endmacro()

##############################################################################
function(NCBI_internal_add_project)

  if (NCBI_EXPERIMENTAL_ENABLE_COLLECTOR AND NOT NCBI_PTBMODE_COLLECT_DEPS)
    list(FIND NCBI_PTB_ALLOWED_PROJECTS ${NCBI_PROJECT} _found)
    if (${_found} LESS "0")
      message("${NCBI_PROJECT} is excluded by user's request")
      if ("${ARGC}" GREATER "0")
        set(${ARGV0} FALSE PARENT_SCOPE)
      endif()
      return()
    endif()
  endif()

if(NCBI_EXPERIMENTAL_VERBOSE OR NCBI_EXPERIMENTAL_VERBOSE_${NCBI_PROJECT})
message("-----------------------------------")
message("NCBI_PROJECT = ${NCBI_PROJECT}")
message("  TYPE = ${NCBI_${NCBI_PROJECT}_TYPE}")
message("  SOURCES = ${NCBI_${NCBI_PROJECT}_SOURCES}")
message("  RESOURCES = ${NCBI_${NCBI_PROJECT}_RESOURCES}")
message("  HEADERS = ${NCBI_${NCBI_PROJECT}_HEADERS}")
message("  REQUIRES = ${NCBI_${NCBI_PROJECT}_REQUIRES}")
message("  COMPONENTS = ${NCBI_${NCBI_PROJECT}_COMPONENTS}")
message("  NOPCH = ${NCBI_${NCBI_PROJECT}_NOPCH}")
message("  NCBILIB = ${NCBI_${NCBI_PROJECT}_NCBILIB}")
message("  EXTLIB = ${NCBI_${NCBI_PROJECT}_EXTLIB}")
message("  INCLUDES = ${NCBI_${NCBI_PROJECT}_INCLUDES}")
message("  DEFINES = ${NCBI_${NCBI_PROJECT}_DEFINES}")
endif()

#  if(NCBI_EXPERIMENTAL_CFG)

    set(NCBITMP_INCLUDES ${NCBI__INCLUDES} ${NCBI_${NCBI_PROJECT}_INCLUDES})
    set(NCBITMP_DEFINES  ${NCBI__DEFINES}  ${NCBI_${NCBI_PROJECT}_DEFINES})
    set(NCBITMP_NCBILIB  ${NCBI__NCBILIB}  ${NCBI_${NCBI_PROJECT}_NCBILIB})
    set(NCBITMP_LIBS     ${NCBI__EXTLIB}  ${NCBI_${NCBI_PROJECT}_EXTLIB})

    NCBI_internal_process_project_requires()
    if ( NOT "${NCBITMP_REQUIRE_NOTFOUND}" STREQUAL "")
      message("${CMAKE_CURRENT_SOURCE_DIR}/${NCBI_PROJECT} is excluded because of unmet requirements: ${NCBITMP_REQUIRE_NOTFOUND}")
      if ("${ARGC}" GREATER "0")
        set(${ARGV0} FALSE PARENT_SCOPE)
      endif()
      return()
    endif()
    if(NCBI_EXPERIMENTAL_VERBOSE OR NCBI_EXPERIMENTAL_VERBOSE_${NCBI_PROJECT})
      message("NCBITMP_INCLUDES = ${NCBITMP_INCLUDES}")
      message("NCBITMP_DEFINES = ${NCBITMP_DEFINES}")
      message("NCBITMP_NCBILIB = ${NCBITMP_NCBILIB}")
      message("NCBITMP_LIBS = ${NCBITMP_LIBS}")
    endif()

    NCBI_internal_process_project_components()
    if ( NOT "${NCBITMP_COMPONENT_NOTFOUND}" STREQUAL "")
      message("${CMAKE_CURRENT_SOURCE_DIR}/${NCBI_PROJECT}: cannot find component: ${NCBITMP_COMPONENT_NOTFOUND}")
    endif()
    if(NCBI_EXPERIMENTAL_VERBOSE OR NCBI_EXPERIMENTAL_VERBOSE_${NCBI_PROJECT})
      message("NCBITMP_INCLUDES = ${NCBITMP_INCLUDES}")
      message("NCBITMP_DEFINES = ${NCBITMP_DEFINES}")
      message("NCBITMP_NCBILIB = ${NCBITMP_NCBILIB}")
      message("NCBITMP_LIBS = ${NCBITMP_LIBS}")
    endif()

    if (NCBI_PTBMODE_COLLECT_DEPS)
      set_property(GLOBAL PROPERTY NCBI_PTBDEPS_${NCBI_PROJECT} ${NCBITMP_NCBILIB})
      get_property(_deps GLOBAL PROPERTY NCBI_PTB_PROJECTS)
      set(_deps ${_deps} ${NCBI_PROJECT})
      set_property(GLOBAL PROPERTY NCBI_PTB_PROJECTS ${_deps})
      if ("${ARGC}" GREATER "0")
        set(${ARGV0} FALSE PARENT_SCOPE)
      endif()
      return()
    endif()
#  endif(NCBI_EXPERIMENTAL_CFG)

  NCBI_internal_collect_sources(${CMAKE_CURRENT_SOURCE_DIR} ${NCBI_PROJECT}) 
  NCBI_internal_collect_headers(${CMAKE_CURRENT_SOURCE_DIR} ${NCBI_PROJECT}) 
  NCBI_internal_add_resources(${NCBI_PROJECT})
  NCBI_internal_add_dataspec(${NCBI_PROJECT})

  if ("${NCBI_${NCBI_PROJECT}_TYPE}" STREQUAL "STATIC")

#message("add_library ${NCBI_PROJECT}")
    set(NCBITMP_DEFINES  ${NCBITMP_DEFINES} "_LIB")
    add_library(${NCBI_PROJECT} STATIC ${NCBITMP_PROJECT_SOURCES} ${NCBITMP_PROJECT_HEADERS} ${NCBITMP_PROJECT_RESOURCES} ${NCBI_${NCBI_PROJECT}_DATASPEC})
    set(_suffix ${CMAKE_STATIC_LIBRARY_SUFFIX})

  elseif ("${NCBI_${NCBI_PROJECT}_TYPE}" STREQUAL "SHARED")

    add_library(${NCBI_PROJECT} SHARED ${NCBITMP_PROJECT_SOURCES} ${NCBITMP_PROJECT_HEADERS} ${NCBITMP_PROJECT_RESOURCES} ${NCBI_${NCBI_PROJECT}_DATASPEC})
    set(_suffix ${CMAKE_SHARED_LIBRARY_SUFFIX})

  elseif ("${NCBI_${NCBI_PROJECT}_TYPE}" STREQUAL "CONSOLEAPP")

#message("add_executable ${NCBI_PROJECT}")
    if(NCBI_EXPERIMENTAL_CFG)
      set(NCBITMP_DEFINES  ${NCBITMP_DEFINES} "_CONSOLE")
      add_executable(${NCBI_PROJECT} ${NCBITMP_PROJECT_SOURCES} ${NCBITMP_PROJECT_HEADERS} ${NCBITMP_PROJECT_RESOURCES} ${NCBI_${NCBI_PROJECT}_DATASPEC})
      set(_suffix ${CMAKE_EXECUTABLE_SUFFIX})
    else()
      add_executable(${NCBI_PROJECT} ${NCBITMP_PROJECT_SOURCES})
    endif()

  else()

    message("${CMAKE_CURRENT_SOURCE_DIR}/${NCBI_PROJECT} unsupported project type ${NCBI_${NCBI_PROJECT}_TYPE}")
    if ("${ARGC}" GREATER "0")
      set(${ARGV0} FALSE PARENT_SCOPE)
    endif()
    return()

  endif()
  if (DEFINED NCBI_${NCBI_PROJECT}_OUTPUT)
    set_target_properties(${NCBI_PROJECT} PROPERTIES OUTPUT_NAME ${NCBI_${NCBI_PROJECT}_OUTPUT})
  endif()

if(NCBI_EXPERIMENTAL_VERBOSE OR NCBI_EXPERIMENTAL_VERBOSE_${NCBI_PROJECT})
message("NCBITMP_PROJECT_SOURCES ${NCBITMP_PROJECT_SOURCES}")
message("NCBITMP_PROJECT_HEADERS ${NCBITMP_PROJECT_HEADERS}")
message("NCBITMP_PROJECT_RESOURCES ${NCBITMP_PROJECT_RESOURCES}")
endif()

  target_include_directories(${NCBI_PROJECT} PRIVATE ${NCBITMP_INCLUDES})
  target_compile_definitions(${NCBI_PROJECT} PRIVATE ${NCBITMP_DEFINES})
#message("target_link_libraries: ${NCBI_PROJECT}     ${NCBITMP_NCBILIB} ${NCBITMP_LIBS}")
  target_link_libraries(     ${NCBI_PROJECT}         ${NCBITMP_NCBILIB} ${NCBITMP_LIBS})

  if (DEFINED _suffix)
    set_target_properties( ${NCBI_PROJECT} PROPERTIES PROJECT_LABEL ${NCBI_PROJECT}${_suffix})
  endif()
  NCBI_internal_define_precompiled_header_usage()

  if (DEFINED NCBI_ALLTESTS)
    foreach(_test IN LISTS NCBI_ALLTESTS)
#message("${NCBI_PROJECT} test = ${_test}, cmd = ${NCBI_${_test}_CMD}, arg = ${NCBI_${_test}_ARG}, dir = ${NCBI_${_test}_DIR}")
      add_test(NAME ${_test} COMMAND ${NCBI_${_test}_CMD} ${NCBI_${_test}_ARG} WORKING_DIRECTORY ${NCBI_${_test}_DIR})
    endforeach()
  endif()

if(NCBI_EXPERIMENTAL_VERBOSE OR NCBI_EXPERIMENTAL_VERBOSE_${NCBI_PROJECT})
message("-----------------------------------")
endif()

  if ("${ARGC}" GREATER "0")
    set(${ARGV0} TRUE PARENT_SCOPE)
  endif()
endfunction()
