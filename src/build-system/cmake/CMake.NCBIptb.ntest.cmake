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
        if(EXISTS ${NCBI_CURRENT_SOURCE_DIR}/${NCBITEST_${_test}_CMD})
            set(NCBITEST_${_test}_ASSETS   ${NCBITEST_${_test}_ASSETS}   ${NCBITEST_${_test}_CMD})
        endif()
    endif()
    set(_requires ${NCBITEST__REQUIRES} ${NCBITEST_${_test}_REQUIRES})
    set(_assets   ${NCBITEST__ASSETS}   ${NCBITEST_${_test}_ASSETS})
    set(_watcher  ${NCBI__WATCHER}      ${NCBI_${NCBI_PROJECT}_WATCHER})
    if (DEFINED NCBITEST_${_test}_TIMEOUT)
        set(_timeout ${NCBITEST_${_test}_TIMEOUT})
    elseif(DEFINED NCBITEST__TIMEOUT)
        set(_timeout ${NCBITEST__TIMEOUT})
    endif()
    string(REPLACE ";" " " _args     "${NCBITEST_${_test}_ARG}")
    string(REPLACE ";" " " _assets   "${_assets}")
    string(REPLACE ";" " " _requires "${_requires}")
    string(REPLACE ";" " " _watcher  "${_watcher}")
    file(RELATIVE_PATH _outdir "${NCBI_THIS_SRC_ROOT}" "${NCBI_CURRENT_SOURCE_DIR}")

    set(_resources "")
    set(_all ${NCBITEST__RESOURCES} ${NCBITEST_${_test}_RESOURCES})
    if (NOT "${_all}" STREQUAL "")
        list(SORT _all)
        list(REMOVE_DUPLICATES _all)
    endif()
    foreach(_res IN LISTS _all)
        if(DEFINED NCBITEST_RESOURCE_${_res}_AMOUNT)
            get_property(_count  GLOBAL PROPERTY NCBITEST_RESOURCE_NTEST_${_res}_COUNT)
            if("${_count}" STREQUAL "")
                set(_count 0)
            endif()
            if(NCBI_VERBOSE_ALLPROJECTS OR NCBI_VERBOSE_PROJECT_${NCBI_PROJECT})
                message("${NCBI_PROJECT} (${NCBI_CURRENT_SOURCE_DIR}): Test ${_test} uses resource ${_res} (${NCBITEST_RESOURCE_${_res}_AMOUNT})")
            endif()
            set(_resources "${_resources} ${_res}_${_count}")
            math(EXPR _count "${_count} + 1")
            if("${_count}" GREATER_EQUAL "${NCBITEST_RESOURCE_${_res}_AMOUNT}")
                set(_count 0)
            endif()
            set_property(GLOBAL PROPERTY NCBITEST_RESOURCE_NTEST_${_res}_COUNT ${_count})
        else()
            if(NCBI_VERBOSE_ALLPROJECTS OR NCBI_VERBOSE_PROJECT_${NCBI_PROJECT})
                message("${NCBI_PROJECT} (${NCBI_CURRENT_SOURCE_DIR}): Test ${_test} uses resource ${_res} (1)")
            endif()
            set(_resources "${_resources} ${_res}")
        endif()
    endforeach()

    set(_s "____")
    string(APPEND _t "${_outdir} ${_s} ${NCBI_PROJECT} ${_s} ${NCBI_${NCBI_PROJECT}_OUTPUT} ${_s} ")
    string(APPEND _t "${NCBITEST_${_test}_CMD} ${_args} ${_s} ")
    string(APPEND _t "${_test} ${_s} ${_assets} ${_s} ${_timeout} ${_s} ")
    string(APPEND _t "${_requires} ${_s} ${_watcher} ${_s} ${_resources} ${_s} ${NCBITEST_${_test}_TESTALIAS}")
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
#    if(NOT "${_checklist}" STREQUAL "")
#        list(SORT _checklist)
#    endif()
    set(_checkdir ${NCBI_DIRNAME_CHECK})
    set(_listdir ${NCBI_BUILD_ROOT}/${_checkdir})
    if (WIN32 OR XCODE)
        set(VALGRIND_PATH "")
    else()
        set(VALGRIND_PATH "valgrind")
    endif()
    set(CHECK_TIMEOUT_MULT 1)
    set(CHECK_OS_NAME "${HOST_OS_DISTR}")
    file(WRITE ${_listdir}/check.sh.list ${_checklist})

    if (EXISTS ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_STATUS})
        file(REMOVE_RECURSE ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_STATUS}/*)
    else()
        file(MAKE_DIRECTORY ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_STATUS})
    endif()
    foreach( _comp IN LISTS NCBI_ALL_COMPONENTS NCBI_ALL_REQUIRES)
#            file(TOUCH ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_STATUS}/${_comp}.enabled)
        file(REMOVE ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_STATUS}/-${_comp}.enabled)
        file(WRITE ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_STATUS}/${_comp}.enabled "")
    endforeach()
    foreach( _comp IN LISTS NCBI_ALL_DISABLED NCBI_ALL_DISABLED_LEGACY)
        file(REMOVE ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_STATUS}/${_comp}.enabled)
        file(WRITE ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_STATUS}/-${_comp}.enabled "")
    endforeach()
    foreach( _comp IN LISTS NCBI_PTBCFG_KNOWN_FEATURES)
        file(REMOVE ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_STATUS}/-${_comp}.enabled)
        file(REMOVE ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_STATUS}/${_comp}.enabled)
    endforeach()
    foreach( _comp IN LISTS NCBI_PTBCFG_PROJECT_FEATURES)
        file(WRITE ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_STATUS}/${_comp}.enabled "")
    endforeach()

    if (EXISTS "${NCBI_TREE_BUILDCFG}/check.cfg.in")
        configure_file(${NCBI_TREE_BUILDCFG}/check.cfg.in ${_listdir}/check.cfg @ONLY)
    endif()
    if (EXISTS "${NCBI_TREE_BUILDCFG}/sysdep.sh.in")
        set(script_shell "#! /bin/sh")
        set(TAIL_N "tail -n ")
        configure_file(${NCBI_TREE_BUILDCFG}/sysdep.sh.in ${NCBI_BUILD_ROOT}/sysdep.sh @ONLY)
        file(COPY ${NCBI_BUILD_ROOT}/sysdep.sh
                DESTINATION ${_listdir}
                FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
        file(REMOVE ${NCBI_BUILD_ROOT}/sysdep.sh)
    endif()
endfunction()

##############################################################################
macro(NCBI_ntest_ToCygwinPath _value _result)
    set(${_result} "${_value}")
    if(WIN32)
        string(FIND ${_value} ":" _pos)
        if(${_pos} EQUAL 1)
            string(REPLACE ":" ""  _tmp "${_value}")
            set(${_result} "/cygdrive/${_tmp}")
        endif()
    endif()
endmacro()

function(NCBI_internal_add_ncbi_checktarget)
    set(SCRIPT_NAME "${NCBITK_TREE_ROOT}/${NCBI_DIRNAME_COMMON_SCRIPTS}/check/check_make_unix_cmake.sh")
    set(WORKDIR ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD})
    set(_checkroot ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_CHECK})
    file(RELATIVE_PATH _checkdir "${WORKDIR}" "${_checkroot}")
    if(NOT EXISTS "${SCRIPT_NAME}")
        message("WARNING: CHECK script not found: ${SCRIPT_NAME}")
        return()
    endif()

    if (WIN32)
        set(_knowndir "C:/Apps/Admin_Installs/Cygwin64/bin;C:/cygwin64/bin;$ENV{PATH}")
        string(REPLACE "\\" "/" _knowndir "${_knowndir}")
        foreach(_dir IN LISTS _knowndir)
            if (NOT "${_dir}" STREQUAL "" AND EXISTS "${_dir}/sh.exe")
                message(STATUS "Found Cygwin: ${_dir}")

                string(REPLACE "/" "\\" _dir "${_dir}")
                NCBI_ntest_ToCygwinPath("${SCRIPT_NAME}" _script)
                NCBI_ntest_ToCygwinPath("${NCBI_THIS_TREE_ROOT}" _root)
                set(_cmdstart set PATH=.$<SEMICOLON>${_dir}$<SEMICOLON>\%PATH\%& set DIAG_SILENT_ABORT=Y&)
                set(_cmdstart ${_cmdstart} sh -c 'set -o igncr$<SEMICOLON>export SHELLOPTS$<SEMICOLON>)
if(OFF)
# on first build, RUN_CHECKS always creates check.sh in both configurations
# subsequent builds do not create check.sh
                foreach(_cfg ${NCBI_CONFIGURATION_TYPES})
                    set(_cmd ${_cmdstart}${_script} ${_checkdir}/check.sh.list ${NCBI_SIGNATURE_${_cfg}} . ${_root} ${_checkdir} check.sh ${_cfg}')
                    add_custom_command(OUTPUT "${_checkroot}/${_cfg}/check.sh"
                        COMMAND ${_cmd}
                        DEPENDS "${_checkroot}/check.sh.list;${SCRIPT_NAME}"
                        WORKING_DIRECTORY ${WORKDIR}
                        COMMENT "Creating ${_checkroot}/${_cfg}/check.sh"
                    )
                endforeach()

                set(_cmd ${_cmdstart}${_checkroot}/$<CONFIG>/check.sh run')
                add_custom_target(RUN_CHECKS
                    COMMAND ${_cmd}
                    DEPENDS "${_checkroot}/$<CONFIG>/check.sh"
                    WORKING_DIRECTORY ${WORKDIR}
                    COMMENT "Running tests"
                    SOURCES "${_checkroot}/check.sh.list;${SCRIPT_NAME}"
                )
elseif(OFF)
# on every build creates check.sh in one configuration, then runs it.
                set(_cmd ${_cmdstart}${_script} ${_checkdir}/check.sh.list ${NCBITEST_SIGNATURE} . ${_root} ${_checkdir} check.sh $<CONFIG>')
                add_custom_target(RUN_CREATE_CHECKS
                    COMMAND ${_cmd}
                    DEPENDS "${_checkroot}/check.sh.list;${SCRIPT_NAME}"
                    SOURCES "${_checkroot}/check.sh.list;${SCRIPT_NAME}"
                    WORKING_DIRECTORY ${WORKDIR}
                    COMMENT "Creating ${_checkroot}/<CONFIG>/check.sh"
                )

                set(_cmd ${_cmdstart}${_checkroot}/$<CONFIG>/check.sh run')
                add_custom_target(RUN_CHECKS
                    COMMAND ${_cmd}
                    DEPENDS "${_checkroot}/$<CONFIG>/check.sh"
                    WORKING_DIRECTORY ${WORKDIR}
                    COMMENT "Running tests"
                )
                add_dependencies(RUN_CHECKS RUN_CREATE_CHECKS)
else()
# on every build creates check.sh in one configuration, then runs it.
                set(_checkname RUN_CHECKS)
                if(NCBI_GENERATOR_IS_MULTI_CONFIG)
                    set(_cmd ${_cmdstart}${_script} ${_checkdir}/check.sh.list ${NCBITEST_SIGNATURE} . ${_root} ${_checkdir} check.sh $<CONFIG>)
                    set(_cmd ${_cmd}$<SEMICOLON>echo Running tests$<SEMICOLON>${_checkroot}/$<CONFIG>/check.sh run)
                    if ($ENV{NCBI_AUTOMATED_BUILD})
                        set(_cmd ${_cmd}$<SEMICOLON>echo Collecting errors$<SEMICOLON>${_checkroot}/$<CONFIG>/check.sh concat_err)
                    endif()
                else()
                    set(_checkname check)
                    set(_cmd ${_cmdstart}${_script} ${_checkdir}/check.sh.list ${NCBITEST_SIGNATURE} . ${_root} ${_checkdir} check.sh .)
                    set(_cmd ${_cmd}$<SEMICOLON>echo Running tests$<SEMICOLON>${_checkroot}/check.sh run)
                    if ($ENV{NCBI_AUTOMATED_BUILD})
                        set(_cmd ${_cmd}$<SEMICOLON>echo Collecting errors$<SEMICOLON>${_checkroot}/check.sh concat_err)
                    endif()
                endif()
                set(_cmd ${_cmd}')
                add_custom_target(${_checkname}
                    COMMAND ${_cmd}
                    DEPENDS "${_checkroot}/check.sh.list;${SCRIPT_NAME}"
                    SOURCES "${_checkroot}/check.sh.list;${SCRIPT_NAME}"
                    WORKING_DIRECTORY ${WORKDIR}
                    COMMENT "Preparing tests"
                )
endif()
                return()
            endif()
        endforeach()
        message("NOT FOUND Cygwin")
    elseif(XCODE)
        set(_cmdstart export DIAG_SILENT_ABORT=Y$<SEMICOLON>)
        set(_checkname RUN_CHECKS)
        if(NCBI_GENERATOR_IS_MULTI_CONFIG)
            set(_cmd ${_cmdstart}${SCRIPT_NAME} ${_checkdir}/check.sh.list ${NCBITEST_SIGNATURE} . ${NCBI_THIS_TREE_ROOT} ${_checkdir} check.sh $<CONFIG>)
            set(_cmd ${_cmd}$<SEMICOLON>echo Running tests$<SEMICOLON>${_checkroot}/$<CONFIG>/check.sh run)
            if ($ENV{NCBI_AUTOMATED_BUILD})
                set(_cmd ${_cmd}$<SEMICOLON>echo Collecting errors$<SEMICOLON>${_checkroot}/$<CONFIG>/check.sh concat_err)
            endif()
        else()
            set(_checkname check)
            set(_cmd ${_cmdstart}${SCRIPT_NAME} ${_checkdir}/check.sh.list ${NCBITEST_SIGNATURE} . ${NCBI_THIS_TREE_ROOT} ${_checkdir} check.sh .)
            set(_cmd ${_cmd}$<SEMICOLON>echo Running tests$<SEMICOLON>${_checkroot}/check.sh run)
            if ($ENV{NCBI_AUTOMATED_BUILD})
                set(_cmd ${_cmd}$<SEMICOLON>echo Collecting errors$<SEMICOLON>${_checkroot}/check.sh concat_err)
            endif()
        endif()
        add_custom_target(${_checkname}
            COMMAND ${_cmd}
            DEPENDS "${_checkroot}/check.sh.list;${SCRIPT_NAME}"
            SOURCES "${_checkroot}/check.sh.list;${SCRIPT_NAME}"
            WORKING_DIRECTORY ${WORKDIR}
            COMMENT "Preparing tests"
        )
    else()
        set(_cmdstart export DIAG_SILENT_ABORT=Y$<SEMICOLON>)
        if(NCBI_GENERATOR_IS_MULTI_CONFIG)
            set(_cmd ${_cmdstart}${SCRIPT_NAME} ${_checkdir}/check.sh.list ${NCBITEST_SIGNATURE} . ${NCBI_THIS_TREE_ROOT} ${_checkdir} check.sh $<CONFIG>)
            set(_cmd ${_cmd}$<SEMICOLON>echo Running tests$<SEMICOLON>${_checkroot}/$<CONFIG>/check.sh run)
            if ($ENV{NCBI_AUTOMATED_BUILD})
                set(_cmd ${_cmd}$<SEMICOLON>echo Collecting errors$<SEMICOLON>${_checkroot}/$<CONFIG>/check.sh concat_err)
            endif()
        else()
            set(_cmd ${_cmdstart}${SCRIPT_NAME} ${_checkdir}/check.sh.list ${NCBITEST_SIGNATURE} . ${NCBI_THIS_TREE_ROOT} ${_checkdir} check.sh$)
            set(_cmd ${_cmd}<SEMICOLON>echo Running tests$<SEMICOLON>${_checkroot}/check.sh run)
            if ($ENV{NCBI_AUTOMATED_BUILD})
                set(_cmd ${_cmd}$<SEMICOLON>echo Collecting errors$<SEMICOLON>${_checkroot}/check.sh concat_err)
            endif()
        endif()
        add_custom_target(check
            COMMAND ${_cmd}
            DEPENDS "${_checkroot}/check.sh.list;${SCRIPT_NAME}"
            WORKING_DIRECTORY ${WORKDIR}
            COMMENT "Preparing tests"
        )
    endif()
endfunction()

#############################################################################
NCBI_register_hook(TARGET_ADDED  NCBI_internal_AddNCBITest)
NCBI_register_hook(ALL_ADDED     NCBI_internal_create_ncbi_checklist)

NCBI_internal_add_ncbi_checktarget()

