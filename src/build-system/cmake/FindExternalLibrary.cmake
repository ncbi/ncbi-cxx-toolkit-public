include(CMakeParseArguments)

function(update_final_message)
    list(GET ARGN 0 ARG_LIBNAME_ORIG)
    if (${ARGC} GREATER 1)
        list(GET ARGN 1 ARG_PRINT_NAME)
    else()
        set(ARG_PRINT_NAME ${ARG_LIBNAME_ORIG})
    endif()
    string(TOUPPER ${ARG_LIBNAME_ORIG} ARG_LIBNAME)
    if ("${ARG_LIBNAME}_DISABLED" STREQUAL "yes")
        set(EXTERNAL_LIBRARIES_COMMENT "${EXTERNAL_LIBRARIES_COMMENT}\n${ARG_PRINT_NAME}: disabled" PARENT_SCOPE)
    elseif(${${ARG_LIBNAME}_FOUND})
        set(EXTERNAL_LIBRARIES_COMMENT "${EXTERNAL_LIBRARIES_COMMENT}\n${ARG_PRINT_NAME}: ${${ARG_LIBNAME}_LIBS}\n${ARG_PRINT_NAME} include: ${${ARG_LIBNAME}_INCLUDE}" PARENT_SCOPE)
    else()
        set(EXTERNAL_LIBRARIES_COMMENT "${EXTERNAL_LIBRARIES_COMMENT}\n${ARG_PRINT_NAME}: not found" PARENT_SCOPE)
    endif()
endfunction()

function(find_external_library)
    list(GET ARGN 0 ARG_LIBNAME_ORIG)
    string(TOUPPER ${ARG_LIBNAME_ORIG} ARG_LIBNAME)
    cmake_parse_arguments(ARG "DO_NOT_UPDATE_MESSAGE" "INCLUDES;LIBS;HINTS;INCLUDE_HINTS;LIBS_HINTS;EXTRAFLAGS;PRINT_NAME" "" ${ARGN})

    if (NOT "${ARG_HINTS}" STREQUAL "")
        set(ARG_INCLUDE_HINTS "${ARG_HINTS}/include")
        set(ARG_LIBS_HINTS "${ARG_HINTS}/lib")
    endif()

    #debug
    #message("ARG_LIBNAME_ORIG ${ARG_LIBNAME_ORIG} LIBNAME ${ARG_LIBNAME} VALUE ${${ARG_LIBNAME}}")
    #message("HINTS ${ARG_HINTS} INCLUDE_HINTS ${ARG_INCLUDE_HINTS} LIBS_HINTS ${ARG_LIBS_HINTS}")
    #message("INCLUDES ${ARG_INCLUDES} LIBS ${ARG_LIBS} EXTRAFLAGS ${ARG_EXTRAFLAGS}")

    if (NOT ARG_LIBNAME)
        message(FATAL_ERROR "find_external_library: library name not provided")
    endif()

    if ("${ARG_LIBNAME}_DISABLED" STREQUAL "yes")
        set("${ARG_LIBNAME}_FOUND" false)
        if (NOT ${ARG_DO_NOT_UPDATE_MESSAGE})
            set(EXTERNAL_LIBRARIES_COMMENT "${EXTERNAL_LIBRARIES_COMMENT}\n${ARG_LIBNAME_ORIG}: disabled" PARENT_SCOPE)
            #update_final_message(${ARG_LIBNAME_ORIG})
        endif()
        return()
    endif()

    #find_library and find_path declare their output variables as  cached (see cmake type system)
    #so, we need to reset them
    unset(EXT_LIBRARY CACHE)
    unset(EXT_INCLUDE_DIR CACHE)

    if (NOT "${${ARG_LIBNAME}}" STREQUAL "")
        find_library(EXT_LIBRARY NAMES ${ARG_LIBS}
            PATH "${${ARG_LIBNAME}}/lib"
            NO_DEFAULT_PATH
            NO_CMAKE_ENVIRONMENT_PATH
            NO_CMAKE_PATH
            NO_SYSTEM_ENVIRONMENT_PATH
            NO_CMAKE_SYSTEM_PATH)
        find_path(EXT_INCLUDE_DIR NAMES ${ARG_INCLUDES}
            PATH "${${ARG_LIBNAME}}/include"
            NO_DEFAULT_PATH
            NO_CMAKE_ENVIRONMENT_PATH
            NO_CMAKE_PATH
            NO_SYSTEM_ENVIRONMENT_PATH
            NO_CMAKE_SYSTEM_PATH)
    elseif (NOT "${ARG_LIBS_HINTS}" STREQUAL "")
        find_library(EXT_LIBRARY NAMES ${ARG_LIBS}
            HINTS
            "${ARG_LIBS_HINTS}")
        find_path(EXT_INCLUDE_DIR NAMES ${ARG_INCLUDES}
            HINTS
            "${ARG_INCLUDE_HINTS}")
    else()
        find_library(EXT_LIBRARY NAMES ${ARG_LIBS})
        find_path(EXT_INCLUDE_DIR NAMES ${ARG_INCLUDES})
    endif()

    #message("EXT_LIBRARY: ${EXT_LIBRARY} EXT_INCLUDE_DIR: ${EXT_INCLUDE_DIR}")
    FIND_PACKAGE_HANDLE_STANDARD_ARGS("${ARG_LIBNAME}"
        REQUIRED_VARS EXT_LIBRARY EXT_INCLUDE_DIR)
    unset(FINAL_LIBS)
    if (${${ARG_LIBNAME}_FOUND})
        set("${ARG_LIBNAME}_INCLUDE" "${EXT_INCLUDE_DIR}" PARENT_SCOPE)
        get_filename_component(LIBRARY_DIR "${EXT_LIBRARY}" DIRECTORY)
        if (NOT ("${LIBRARY_DIR}" STREQUAL "/usr/lib" OR "${LIBRARY_DIR}" STREQUAL "/usr/lib64"))
            set(PATH_FLAGS "-L${LIBRARY_DIR} -Wl,-rpath,${LIBRARY_DIR} ")
        endif()
        if ("${ARG_EXTRAFLAGS}" STREQUAL "")
            set(FINAL_LIBS "${PATH_FLAGS}-l${ARG_LIBS}")
        else ()
            set(FINAL_LIBS "${PATH_FLAGS}-l${ARG_LIBS} ${ARG_EXTRAFLAGS}")
        endif()
        set("${ARG_LIBNAME}_LIBS" "${FINAL_LIBS}" PARENT_SCOPE)
        set("${ARG_LIBNAME}_LIBRARIES" "${LIBRARY_DIR}" PARENT_SCOPE)
        if (NOT ${ARG_DO_NOT_UPDATE_MESSAGE})
            set(EXTERNAL_LIBRARIES_COMMENT "${EXTERNAL_LIBRARIES_COMMENT}\n${ARG_LIBNAME_ORIG}: ${FINAL_LIBS}\n${ARG_LIBNAME_ORIG} include: ${EXT_INCLUDE_DIR}" PARENT_SCOPE)
        endif()
        set(TOOLKIT_MODULES_FOUND "${TOOLKIT_MODULES_FOUND} ${ARG_LIBNAME_ORIG}" PARENT_SCOPE)
    elseif (NOT ${ARG_DO_NOT_UPDATE_MESSAGE})
        set(EXTERNAL_LIBRARIES_COMMENT "${EXTERNAL_LIBRARIES_COMMENT}\n${ARG_LIBNAME_ORIG}: not found" PARENT_SCOPE)
    endif()
    set("${ARG_LIBNAME}_FOUND" ${${ARG_LIBNAME}_FOUND} PARENT_SCOPE)
    #if (NOT ${ARG_DO_NOT_UPDATE_MESSAGE})
    #update_final_message(${ARG_LIBNAME_ORIG})
    #endif()
    if (${${ARG_LIBNAME}_FOUND})
        message(STATUS "Found ${ARG_LIBNAME_ORIG}: ${FINAL_LIBS}")
    else()
        message(STATUS "Could NOT find ${ARG_LIBNAME_ORIG}")
    endif()

endfunction()
