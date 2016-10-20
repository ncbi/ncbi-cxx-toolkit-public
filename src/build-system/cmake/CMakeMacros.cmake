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


function( CheckVersionControl generated)
    set(local_generated)
    foreach (f IN LISTS ARGV)
        set(retcode)
        execute_process(
            COMMAND /usr/bin/svn info "${f}"
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


macro( add_subdirectory_optional SUBDIR)
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR}/CMakeLists.txt )
        add_subdirectory(${SUBDIR})
    endif()
endmacro(add_subdirectory_optional)



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

    add_custom_command(
        OUTPUT ${generated}
        COMMAND ${NCBI_DATATOOL} -oR \"${top_src_dir}\" -opm \"${MODULE_SEARCH}\" -m \"${MODULE_PATH}\" -M \""${MODULE_IMPORT}"\" -oA -oc \"${MODULE}\" -or \"${MODULE_PATH_RELATIVE}\" -odi -od \"${SOURCE_PATH}/${MODULE}.def\" -oex '' -ocvs -pch \"ncbi_pch.hpp\" -fd ${MODULE}.dump
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating module: ${MODULE}"
        DEPENDS ${NCBI_DATATOOL} ${MODULE_PATH}
        )

endmacro( RunDatatool )
