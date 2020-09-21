##############################################################################
##
## General wrapper for find_package()
## This wrapper implements some overrides to search for NCBI-specific package
## overrides.  We maintain a directory of configs that indicate the specific
## layout of libraries for development at NCBI.
##

if(OFF)
macro(find_package arg_mod)
    # uncomment this for extensive logging
    #set(_NCBI_MODULE_DEBUG True)
    set(_MOD_CONF1 "NCBI-${arg_mod}Config.cmake")
    string(TOLOWER "NCBI-${arg_mod}-config.cmake" _MOD_CONF2)
    #message(STATUS "Checking for config: ${_NCBI_DEFAULT_PACKAGE_SEARCH_PATH}/${_MOD_CONF1}")
    #message(STATUS "Checking for config: ${_NCBI_DEFAULT_PACKAGE_SEARCH_PATH}/${_MOD_CONF2}")
    if (EXISTS "${_NCBI_DEFAULT_PACKAGE_SEARCH_PATH}/${_MOD_CONF1}"
            OR EXISTS "${_NCBI_DEFAULT_PACKAGE_SEARCH_PATH}/${_MOD_CONF2}")
        _find_package(${arg_mod} ${ARGN}
            NAMES NCBI-${arg_mod}
            )
        if (${${arg_mod}_FOUND})
            if (${${arg_mod}_VERSION_STRING})
                message(STATUS "Found ${arg_mod} (NCBI config): ${${arg_mod}_LIBRARIES} (version \"${${arg_mod}_VERSION_STRING}\")")
            else()
                message(STATUS "Found ${arg_mod} (NCBI config): ${${arg_mod}_LIBRARIES}")
            endif()
        endif()
    else()
        _find_package(${arg_mod} ${ARGN})
        if (${${arg_mod}_FOUND})
            if (${${arg_mod}_VERSION_STRING})
                message(STATUS "Found ${arg_mod} (CMake): ${${arg_mod}_LIBRARIES} (found version \"${${arg_mod}_VERSION_STRING}\")")
            else()
                message(STATUS "Found ${arg_mod} (CMake): ${${arg_mod}_LIBRARIES}")
            endif()
        endif()
    endif()

    if (${${arg_mod}_FOUND})
        list(APPEND NCBI_MODULES_FOUND ${arg_mod})

        #
        # everything below is an insulation layer
        # this is designed to make the standard C++ toolkit-type macros work
        #

        string(TOUPPER ${arg_mod} _arg_mod_upper)
        if (DEFINED ${arg_mod}_LIBRARIES)
            set(${_arg_mod_upper}_LIBS ${${arg_mod}_LIBRARIES})
        elseif(DEFINED ${arg_mod}_LIBRARY)
            set(${_arg_mod_upper}_LIBS ${${arg_mod}_LIBRARY})
        elseif (DEFINED ${_arg_mod_upper}_LIBRARIES)
            set(${_arg_mod_upper}_LIBS ${${_arg_mod_upper}_LIBRARIES})
        elseif(DEFINED ${_arg_mod_upper}_LIBRARY)
            set(${_arg_mod_upper}_LIBS ${${_arg_mod_upper}_LIBRARY})
        endif()

        if (DEFINED ${arg_mod}_INCLUDE_DIRS)
            set(${_arg_mod_upper}_INCLUDE ${${arg_mod}_INCLUDE_DIRS})
        elseif(DEFINED ${arg_mod}_INCLUDE_DIR)
            set(${_arg_mod_upper}_INCLUDE ${${arg_mod}_INCLUDE_DIR})
        elseif (DEFINED ${_arg_mod_upper}_INCLUDE_DIRS)
            set(${_arg_mod_upper}_INCLUDE ${${_arg_mod_upper}_INCLUDE_DIRS})
        elseif(DEFINED ${_arg_mod_upper}_INCLUDE_DIR)
            set(${_arg_mod_upper}_INCLUDE ${${_arg_mod_upper}_INCLUDE_DIR})
        endif()

        if (_NCBI_MODULE_DEBUG)
            message(STATUS "  FindPackage(): ${_arg_mod_upper}_INCLUDE = ${${_arg_mod_upper}_INCLUDE}")
            message(STATUS "  FindPackage(): ${_arg_mod_upper}_LIBS = ${${_arg_mod_upper}_LIBS}")
        endif()
    endif()
endmacro()
endif()

##############################################################################
##
## Recurse a directory, but only if the directory actually exists
## This is useful for marking optional elements that may or may not be present
## in meta-trees
##

macro( add_subdirectory_optional SUBDIR)
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR}/CMakeLists.txt )
        add_subdirectory(${SUBDIR})
    endif()
endmacro(add_subdirectory_optional)




##############################################################################
##
## Macros for working with 'datatool'
##


macro( ReadModuleSpec MODULE_DEFS_FILE )
    if (EXISTS "${MODULE_DEFS_FILE}")
        #message("Processing module file: ${MODULE_DEFS_FILE}")
        FILE(READ "${MODULE_DEFS_FILE}" contents)

        #STRING(REGEX MATCH "MODULE_PATH *=[^\n]*[^ \n]" MODULE_DIRECTORY "${contents}")
        #STRING(REGEX REPLACE "MODULE_PATH *= *" " " MODULE_DIRECTORY "${MODULE_DIRECTORY}")

        STRING(REGEX MATCH "MODULE_IMPORT *=[^\n]*[^ \n]" _TMP "${contents}")
        STRING(REGEX REPLACE "MODULE_IMPORT *= *" "" _TMP "${_TMP}")
        STRING(REGEX REPLACE "  *$" "" _TMP_LIST "${_TMP}")
        STRING(REGEX REPLACE " " ";" _TMP_LIST "${_TMP}")
        #message("list=${_TMP_LIST}")
        set(MODULE_IMPORT "")

        foreach(mod ${_TMP_LIST})
            #message("MODULE_IMPORT=|${MODULE_IMPORT}|  mod=|${mod}|")
            set(MODULE_IMPORT "${MODULE_IMPORT} ${mod}.asn")
        endforeach(mod)

        #message("module path = ${MODULE_PATH}")
        #message("module import = ${MODULE_IMPORT}")
    else()
        #message("No module file: ${MODULE_DEFS_FILE}")
    endif()
endmacro( ReadModuleSpec )


##############################################################################
##
## Test whether a file is maintained under version control
## Note: This is not currently used
##
function( CheckVersionControl generated)
    set(local_generated)
    foreach (f IN LISTS ARGV)
        set(retcode)
        execute_process(
            COMMAND ${Subversion_SVN_EXECUTABLE} info "${f}"
            RESULT_VARIABLE retcode
            OUTPUT_VARIABLE _foo_output
            ERROR_VARIABLE _foo_error
            )
        if (NOT retcode EQUAL 0)
            list(APPEND local_generated ${f})
        endif()
    endforeach(f)
    set(generated_files )
    list(APPEND generated_files ${local_generated})
    set(${generated_files} ${${generated_files}} PARENT_SCOPE)
endfunction(CheckVersionControl)



##############################################################################
##
## Extract the name of the client (file names for the client code) for
## datatool-generated RPC clients
## The client name is a field in the definitions
##
macro( GetClientName MODULE )
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}.def")

        FILE(READ "${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}.def" contents)
        STRING(REGEX MATCH "clients *=[^\n]*[^ \n]" CLIENT "${contents}")
        STRING(REGEX REPLACE "clients *= *" "" CLIENT "${CLIENT}")
        if ("${CLIENT}" STREQUAL "")
        else()
            set(client1 "${CMAKE_CURRENT_SOURCE_DIR}/${CLIENT}.cpp")
            set(client2 "${CMAKE_CURRENT_SOURCE_DIR}/${CLIENT}_.cpp")
            get_filename_component(client1 "${client1}" REALPATH)
            get_filename_component(client2 "${client2}" REALPATH)

            # test if our files are under version control
            # if they are, then these
            #set(CLIENT ${client1} ${client2})
            set(CLIENT ${client2})
            if (NOT EXISTS "${client1}")
                set(CLIENT ${CLIENT} ${client1})
            endif()
            #message("client is ${CLIENT}")
        endif()
    endif()
endmacro( GetClientName )

##############################################################################
##
## The main driver for executing datatool
##

macro( RunDatatool MODULE MODULE_SEARCH )
    if ("${MODULE_EXT}" STREQUAL "")
        set(MODULE_EXT "asn")
    endif()

    if ("${MODULE_PATH_RELATIVE}" STREQUAL "")
        #set(MODULE_PATH_RELATIVE "objects/${MODULE}")
        get_filename_component(MODULE_PATH_RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" REALPATH)
        get_filename_component(tmp "${top_src_dir}/src" REALPATH)
        string(REPLACE "${tmp}/" "" MODULE_PATH_RELATIVE "${MODULE_PATH_RELATIVE}")
    endif()

    foreach(mod ${MODULE_IMPORT})
        set(LOCAL_MODULE_IMPORT ${LOCAL_MODULE_IMPORT} ${mod}.${MODULE_EXT})
    endforeach(mod)


    #message("  searching for: ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}.${MODULE_EXT}")
    set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}.${MODULE_EXT})
    set(MODULE_DEFS_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}.module)
    if (NOT EXISTS "${MODULE_PATH}")
        #message("  searching for: ${CMAKE_CURRENT_SOURCE_DIR}/../${MODULE}/${MODULE}.${MODULE_EXT}")
        set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../${MODULE}/${MODULE}.${MODULE_EXT})
        set(MODULE_DEFS_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../${MODULE}/${MODULE}.module)
    endif()
    if (NOT EXISTS "${MODULE_PATH}")
        #message("  searching for: ${CMAKE_CURRENT_SOURCE_DIR}/../../${MODULE_PATH_RELATIVE}/${MODULE}.${MODULE_EXT}")
        set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../${MODULE_PATH_RELATIVE}/${MODULE}.${MODULE_EXT})
        set(MODULE_DEFS_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../../${MODULE_PATH_RELATIVE}/${MODULE}.module)
    endif()
    if (NOT EXISTS "${MODULE_PATH}")
        message(SEND_ERROR "The requested module '${MODULE}' cannot be found")
    endif()

    get_filename_component(MODULE_PATH "${MODULE_PATH}" REALPATH)
    get_filename_component(MODULE_DEFS_FILE "${MODULE_DEFS_FILE}" REALPATH)

    ReadModuleSpec( "${MODULE_DEFS_FILE}")
    GetClientName("${MODULE}")

    get_filename_component(SOURCE_PATH "${MODULE_PATH}" PATH)
    get_filename_component(tmp__cpp "${SOURCE_PATH}/${MODULE}__.cpp" REALPATH)
    get_filename_component(tmp___cpp "${SOURCE_PATH}/${MODULE}___.cpp" REALPATH)

    set(generated)
    list(APPEND generated ${tmp__cpp} ${tmp___cpp} ${CLIENT})
    CheckVersionControl(${generated})

    #message("module = ${MODULE}")
    #message("  module_path          = ${MODULE_PATH}")
    #message("  relative module_path = ${MODULE_PATH_RELATIVE}")
    #message("  source_path          = ${SOURCE_PATH}")
    #message("  generated files      = ${generated}")


    SET_SOURCE_FILES_PROPERTIES(${tmp__cpp} ${tmp___cpp} ${CLIENT} PROPERTIES GENERATED 1)
    #SET_TARGET_PROPERTIES(${MODULE} PROPERTIES LINKER_LANGUAGE CXX)
    SET(NEW_MODULE_FILES ${SOURCE_PATH}/${MODULE}__.cpp ${SOURCE_PATH}/${MODULE}___.cpp)

    if (WIN32)
        add_custom_command(
            OUTPUT ${generated}
            COMMAND ${NCBI_DATATOOL} -oR "${top_src_dir}" -opm "${MODULE_SEARCH}" -m "${MODULE_PATH}" -M "${MODULE_IMPORT}" -oA -oc "${MODULE}" -or "${MODULE_PATH_RELATIVE}" -odi -od "${SOURCE_PATH}/${MODULE}.def" -ocvs     -pch "ncbi_pch.hpp" -fd ${MODULE}.dump VERBATIM
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Generating module: ${MODULE}"
            DEPENDS ${NCBI_DATATOOL} ${MODULE_PATH}
            VERBATIM
        )
    else()
        add_custom_command(
            OUTPUT ${generated}
            COMMAND ${NCBI_DATATOOL} -oR \"${top_src_dir}\" -opm \"${MODULE_SEARCH}\" -m \"${MODULE_PATH}\" -M \""${MODULE_IMPORT}"\" -oA -oc \"${MODULE}\" -or \"${MODULE_PATH_RELATIVE}\" -odi -od \"${SOURCE_PATH}/${MODULE}.def\" -ocvs -pch \"ncbi_pch.hpp\" -fd ${MODULE}.dump
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Generating module: ${MODULE}"
            DEPENDS ${NCBI_DATATOOL} ${MODULE_PATH}
        )
    endif()

endmacro( RunDatatool )

