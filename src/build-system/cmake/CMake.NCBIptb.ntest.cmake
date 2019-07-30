#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper extension
##  In NCBI CMake wrapper, adds NCBI tests (which use NCBI testing framework)
##    Author: Andrei Gourianov, gouriano@ncbi
##

##############################################################################
function(NCBI_internal_add_ncbi_test _test)
    if( NOT DEFINED NCBITEST_${_test}_CMD)
        set(NCBITEST_${_test}_CMD ${NCBI_${NCBI_PROJECT}_OUTPUT})
    endif()
    get_filename_component(_ext ${NCBITEST_${_test}_CMD} EXT)
    if("${_ext}" STREQUAL ".sh")
        set(NCBITEST_${_test}_ASSETS   ${NCBITEST_${_test}_ASSETS}   ${NCBITEST_${_test}_CMD})
    endif()
    set(_requires ${NCBITEST__REQUIRES} ${NCBITEST_${_test}_REQUIRES})
    set(_assets   ${NCBITEST__ASSETS}   ${NCBITEST_${_test}_ASSETS})
    if (DEFINED NCBITEST_${_test}_TIMEOUT)
        set(_timeout ${NCBITEST_${_test}_TIMEOUT})
    elseif(DEFINED NCBITEST__TIMEOUT)
        set(_timeout ${NCBITEST__TIMEOUT})
    endif()
    string(REPLACE ";" " " _args     "${NCBITEST_${_test}_ARG}")
    string(REPLACE ";" " " _assets   "${_assets}")
    string(REPLACE ";" " " _requires "${_requires}")
    string(REPLACE ";" " " _watcher  "${NCBI_${NCBI_PROJECT}_WATCHER}")
    file(RELATIVE_PATH _outdir "${NCBI_SRC_ROOT}" "${NCBI_CURRENT_SOURCE_DIR}")

    set(_s "____")
    string(APPEND _t "${_outdir} ${_s} ${NCBI_PROJECT} ${_s} ${NCBI_${NCBI_PROJECT}_OUTPUT} ${_s} ")
    string(APPEND _t "${NCBITEST_${_test}_CMD} ${_args} ${_s} ")
    string(APPEND _t "${_test} ${_s} ${_assets} ${_s} ${_timeout} ${_s} ")
    string(APPEND _t "${_requires} ${_s} ${_watcher}")
    get_property(_checklist GLOBAL PROPERTY NCBI_PTBPROP_CHECKLIST)
    LIST(APPEND _checklist "${_t}\n")
    set_property(GLOBAL PROPERTY NCBI_PTBPROP_CHECKLIST ${_checklist})
endfunction()

##############################################################################
function(NCBI_internal_AddNCBITest _variable _access)
    if("${_access}" STREQUAL "MODIFIED_ACCESS" AND DEFINED NCBI_${NCBI_PROJECT}_ALLTESTS)
        foreach(_test IN LISTS NCBI_${NCBI_PROJECT}_ALLTESTS)
            NCBI_internal_add_ncbi_test(${_test})
        endforeach()
    endif()
endfunction()

##############################################################################
function(NCBI_internal_create_ncbi_checklist _variable _access)
    if(NOT "${_access}" STREQUAL "MODIFIED_ACCESS")
        return()
    endif()

    get_property(_checklist GLOBAL PROPERTY NCBI_PTBPROP_CHECKLIST)
    if(NOT "${_checklist}" STREQUAL "")
        list(SORT _checklist)
        if (WIN32 OR XCODE)
            set(_listdir ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}/${CMAKE_PROJECT_NAME}.check)
            foreach(_cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
                file(WRITE ${_listdir}/${_cfg}/check.sh.list ${_checklist})
            endforeach()
        else()
            set(_listdir ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD})
            file(WRITE ${_listdir}/check.sh.list ${_checklist})

            file(REMOVE_RECURSE ${NCBI_BUILD_ROOT}/status)
            foreach( _comp IN LISTS NCBI_ALL_COMPONENTS)
                file(WRITE ${NCBI_BUILD_ROOT}/status/${_comp}.enabled "")
            endforeach()

            if (EXISTS "${NCBI_TREE_BUILDCFG}/check.cfg.in")
                set(CHECK_TIMEOUT_MULT 1)
                set(VALGRIND_PATH "valgrind")
                set(CHECK_OS_NAME "${CMAKE_HOST_SYSTEM}")
                configure_file(${NCBI_TREE_BUILDCFG}/check.cfg.in ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}/check.cfg @ONLY)
            endif()
            if (EXISTS "${NCBI_TREE_BUILDCFG}/sysdep.sh.in")
                set(script_shell "#! /bin/sh")
                set(TAIL_N "tail -n ")
                configure_file(${NCBI_TREE_BUILDCFG}/sysdep.sh.in ${NCBI_BUILD_ROOT}/sysdep.sh @ONLY)
                file(COPY ${NCBI_BUILD_ROOT}/sysdep.sh
                     DESTINATION ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}
                     FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
                file(REMOVE ${NCBI_BUILD_ROOT}/sysdep.sh)
            endif()

        endif()
    endif()
endfunction()

##############################################################################
function(NCBI_internal_add_ncbi_checktarget)
    if(DEFINED NCBI_EXTERNAL_TREE_ROOT)
        set(SCRIPT_NAME "${NCBI_EXTERNAL_TREE_ROOT}/${NCBI_DIRNAME_COMMON_SCRIPTS}/check/check_make_unix.sh")
    else()
        set(SCRIPT_NAME "${NCBI_TREE_ROOT}/${NCBI_DIRNAME_COMMON_SCRIPTS}/check/check_make_unix.sh")
    endif()
    set(WORKDIR ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD})
    get_filename_component(_build ${NCBI_BUILD_ROOT} NAME)

    add_custom_target(check
        COMMAND ${SCRIPT_NAME} check.sh.list ${_build} ${WORKDIR} ${NCBI_TREE_ROOT} ${WORKDIR} check.sh
        COMMAND ${CMAKE_COMMAND} -E echo "To run tests: cd ${WORKDIR}\; ./check.sh run"
        DEPENDS ${SCRIPT_NAME}
        SOURCES ${SCRIPT_NAME}
        WORKING_DIRECTORY ${WORKDIR}
    )
endfunction()

#############################################################################
NCBI_register_hook(TARGET_ADDED  NCBI_internal_AddNCBITest)
NCBI_register_hook(ALL_ADDED     NCBI_internal_create_ncbi_checklist)

if(NOT WIN32 AND NOT XCODE)
    NCBI_internal_add_ncbi_checktarget()
endif()

