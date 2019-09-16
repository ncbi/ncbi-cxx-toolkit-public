#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper extension
##  In NCBI CMake wrapper, adds NCBI-specific build parameters
##    Author: Andrei Gourianov, gouriano@ncbi
##


##############################################################################
function(NCBI_internal_add_NCBI_definitions)
    set(_defs "")
    if (${NCBI_${NCBI_PROJECT}_TYPE} STREQUAL "STATIC")
        if (WIN32)
            set(_defs "_LIB")
        endif()
    elseif (${NCBI_${NCBI_PROJECT}_TYPE} STREQUAL "SHARED")
        if (WIN32)
            set(_defs "_USRDLL")
        endif()
    elseif (${NCBI_${NCBI_PROJECT}_TYPE} STREQUAL "CONSOLEAPP")
        if (WIN32)
            set(_defs "_CONSOLE")
        endif()
        if (DEFINED NCBI_${NCBI_PROJECT}_OUTPUT)
            set(_defs  ${_defs} NCBI_APP_BUILT_AS=${NCBI_${NCBI_PROJECT}_OUTPUT})
        else()
            set(_defs  ${_defs} NCBI_APP_BUILT_AS=${NCBI_PROJECT})
        endif()
    endif()
    if(NOT "${_defs}" STREQUAL "")
        target_compile_definitions(${NCBI_PROJECT} PRIVATE ${_defs})
    endif()
endfunction()

#############################################################################
NCBI_register_hook(TARGET_ADDED NCBI_internal_add_NCBI_definitions)
