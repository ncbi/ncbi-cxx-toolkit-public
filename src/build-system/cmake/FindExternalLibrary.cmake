include(CMakeParseArguments)

function(find_external_library)
  list(GET ARGN 0 ARG_LIBNAME_ORIG)
  string(TOUPPER ${ARG_LIBNAME_ORIG} ARG_LIBNAME)
  cmake_parse_arguments(ARG "" "INCLUDES;LIBS;HINTS;INCLUDE_HINTS;LIBS_HINTS;EXTRAFLAGS" "" ${ARGN})

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
    set(EXTERNAL_LIBRARIES_COMMENT "${EXTERNAL_LIBRARIES_COMMENT}\n${ARG_LIBNAME_ORIG}: disabled" PARENT_SCOPE)
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
  if ("${ARG_LIBNAME}_FOUND")
    set("${ARG_LIBNAME}_INCLUDE" "${EXT_INCLUDE_DIR}" PARENT_SCOPE)
    get_filename_component(LIBRARY_DIR "${EXT_LIBRARY}" DIRECTORY)
    if ("${ARG_EXTRAFLAGS}" STREQUAL "")
      set(FINAL_LIBS "-L${LIBRARY_DIR} -Wl,-rpath,${LIBRARY_DIR} -l${ARG_LIBS}")
    else ()
      set(FINAL_LIBS "-L${LIBRARY_DIR} -Wl,-rpath,${LIBRARY_DIR} -l${ARG_LIBS} ${ARG_EXTRAFLAGS}")
    endif()
    set("${ARG_LIBNAME}_LIBS" "${FINAL_LIBS}" PARENT_SCOPE)
    set(EXTERNAL_LIBRARIES_COMMENT "${EXTERNAL_LIBRARIES_COMMENT}\n${ARG_LIBNAME_ORIG}: ${FINAL_LIBS}\n${ARG_LIBNAME_ORIG} include: ${EXT_INCLUDE_DIR}" PARENT_SCOPE)
  else()
    set(EXTERNAL_LIBRARIES_COMMENT "${EXTERNAL_LIBRARIES_COMMENT}\n${ARG_LIBNAME_ORIG}: not found" PARENT_SCOPE)
  endif()
  set("${ARG_LIBNAME}_FOUND" ${${ARG_LIBNAME}_FOUND} PARENT_SCOPE)
endfunction()
