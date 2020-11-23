#############################################################################
# $Id$
#############################################################################
##
##  NCBI CMake wrapper extension
##  In NCBI CMake wrapper, adds CMake tests (which use CMake testing framework)
##    Author: Andrei Gourianov, gouriano@ncbi
##


##############################################################################
# Testing

set(NCBITEST_DRIVER "${NCBI_DIRNAME_CMAKECFG}/TestDriver.cmake")

NCBI_define_test_resource(ServiceMapper 8)
enable_testing()

if (WIN32)
    set(_knowndir "C:/Apps/Admin_Installs/Cygwin64/bin;C:/cygwin64/bin;$ENV{PATH}")
    string(REPLACE "\\" "/" _knowndir "${_knowndir}")
    foreach(_dir IN LISTS _knowndir)
        if (NOT "${_dir}" STREQUAL "" AND EXISTS "${_dir}/sh.exe")
            set(NCBI_REQUIRE_CygwinTest_FOUND YES)
            set(NCBI_CygwinTest_PATH "${_dir}")
            break()
        endif()
    endforeach()
endif()

##############################################################################

macro(NCBI_internal_process_cmake_test_requires _test)
    set(NCBITEST_REQUIRE_NOTFOUND "")
    set(_all ${NCBITEST__REQUIRES} ${NCBITEST_${_test}_REQUIRES})
    if (NOT "${_all}" STREQUAL "")
        list(REMOVE_DUPLICATES _all)
    endif()

    foreach(_req IN LISTS _all)
        NCBI_util_parse_sign( ${_req} _value _negate)
        if (${_value} OR NCBI_REQUIRE_${_value}_FOUND OR NCBI_COMPONENT_${_value}_FOUND)
            if (_negate)
                set(NCBITEST_REQUIRE_NOTFOUND ${NCBITEST_REQUIRE_NOTFOUND} ${_req})
            endif()
        else()
            if (NOT _negate)
                set(NCBITEST_REQUIRE_NOTFOUND ${NCBITEST_REQUIRE_NOTFOUND} ${_req})
            endif()
        endif()     
    endforeach()
endmacro()

##############################################################################

macro(NCBI_internal_process_cmake_test_resources _test)
    set(_all ${NCBITEST__RESOURCES} ${NCBITEST_${_test}_RESOURCES})
    if (NOT "${_all}" STREQUAL "")
        list(REMOVE_DUPLICATES _all)
    endif()

    foreach(_res IN LISTS _all)
        if("${_res}" STREQUAL "SERIAL")
            if(NCBI_VERBOSE_ALLPROJECTS OR NCBI_VERBOSE_PROJECT_${NCBI_PROJECT})
                message("${NCBI_PROJECT} (${NCBI_CURRENT_SOURCE_DIR}): Test ${_test} uses resource SERIAL (will run alone)")
            endif()
            set_tests_properties(${_test} PROPERTIES RUN_SERIAL "TRUE")
        elseif(DEFINED NCBITEST_RESOURCE_${_res}_AMOUNT)
            get_property(_count  GLOBAL PROPERTY NCBITEST_RESOURCE_CTEST_${_res}_COUNT)
            if("${_count}" STREQUAL "")
                set(_count 0)
            endif()
            if(NCBI_VERBOSE_ALLPROJECTS OR NCBI_VERBOSE_PROJECT_${NCBI_PROJECT})
                message("${NCBI_PROJECT} (${NCBI_CURRENT_SOURCE_DIR}): Test ${_test} uses resource ${_res} (${NCBITEST_RESOURCE_${_res}_AMOUNT})")
            endif()
            set_tests_properties(${_test} PROPERTIES RESOURCE_LOCK "NCBITEST_RESOURCE_${_res}_${_count}")
            math(EXPR _count "${_count} + 1")
            if("${_count}" GREATER_EQUAL "${NCBITEST_RESOURCE_${_res}_AMOUNT}")
                set(_count 0)
            endif()
            set_property(GLOBAL PROPERTY NCBITEST_RESOURCE_CTEST_${_res}_COUNT ${_count})
        else()
            if(NCBI_VERBOSE_ALLPROJECTS OR NCBI_VERBOSE_PROJECT_${NCBI_PROJECT})
                message("${NCBI_PROJECT} (${NCBI_CURRENT_SOURCE_DIR}): Test ${_test} uses resource ${_res} (1)")
            endif()
            set_tests_properties(${_test} PROPERTIES RESOURCE_LOCK "NCBITEST_RESOURCE_${_res}")
        endif()
    endforeach()
endmacro()

##############################################################################

macro(NCBI_internal_process_cmake_test_labels _test)
    set(_all ${NCBI__PROJTAG} ${NCBI_${NCBI_PROJECT}_PROJTAG} ${NCBITEST__LABELS} ${NCBITEST_${_test}_LABELS})
    if (NOT "${_all}" STREQUAL "")
        list(REMOVE_DUPLICATES _all)
        if(NCBI_VERBOSE_ALLPROJECTS OR NCBI_VERBOSE_PROJECT_${NCBI_PROJECT})
            message("${NCBI_PROJECT} (${NCBI_CURRENT_SOURCE_DIR}): Test ${_test} labels = ${_all}")
        endif()
        set_tests_properties(${_test} PROPERTIES LABELS "${_all}")
    endif()
endmacro()

##############################################################################

function(NCBI_internal_add_cmake_test _test)
    if( NOT DEFINED NCBITEST_${_test}_CMD)
        set(NCBITEST_${_test}_CMD ${NCBI_${NCBI_PROJECT}_OUTPUT})
    endif()
    if (XCODE)
        set(_extra -DXCODE=TRUE)
    endif()
    get_filename_component(_ext ${NCBITEST_${_test}_CMD} EXT)
    if("${_ext}" STREQUAL ".sh" OR "${_ext}" STREQUAL ".bash")
        if (WIN32)
            set(NCBITEST_${_test}_REQUIRES ${NCBITEST_${_test}_REQUIRES} CygwinTest)
            if(NCBI_REQUIRE_CygwinTest_FOUND)
                set(_extra ${_extra} -DNCBITEST_CYGWIN=${NCBI_CygwinTest_PATH})
            endif()
        endif()
        if(EXISTS ${NCBI_CURRENT_SOURCE_DIR}/${NCBITEST_${_test}_CMD})
            set(NCBITEST_${_test}_ASSETS   ${NCBITEST_${_test}_ASSETS}   ${NCBITEST_${_test}_CMD})
        endif()
    elseif($ENV{NCBI_AUTOMATED_BUILD})
        if(NCBI_REQUIRE_CygwinTest_FOUND AND WIN32)
            set(_extra ${_extra} -DNCBITEST_CYGWIN=${NCBI_CygwinTest_PATH})
        endif()
    endif()
    set(_assets  ${NCBITEST__ASSETS} ${NCBITEST_${_test}_ASSETS})
    set(_watcher ${NCBI__WATCHER}    ${NCBI_${NCBI_PROJECT}_WATCHER})
    if (DEFINED NCBITEST_${_test}_TIMEOUT)
        set(_timeout ${NCBITEST_${_test}_TIMEOUT})
    elseif(DEFINED NCBITEST__TIMEOUT)
        set(_timeout ${NCBITEST__TIMEOUT})
    else()
        set(_timeout 200)
    endif()
    string(REPLACE ";" " " _args    "${NCBITEST_${_test}_ARG}")
    string(REPLACE ";" " " _assets  "${_assets}")
    string(REPLACE ";" " " _watcher "${_watcher}")

    NCBI_internal_process_cmake_test_requires(${_test})
    if ( NOT "${NCBITEST_REQUIRE_NOTFOUND}" STREQUAL "")
        if(NCBI_VERBOSE_ALLPROJECTS OR NCBI_VERBOSE_PROJECT_${NCBI_PROJECT})
            message("${NCBI_PROJECT} (${NCBI_CURRENT_SOURCE_DIR}): Test ${_test} is excluded because of unmet requirements: ${NCBITEST_REQUIRE_NOTFOUND}")
        endif()
        return()
    endif()

    file(RELATIVE_PATH _xoutdir "${NCBI_SRC_ROOT}" "${NCBI_CURRENT_SOURCE_DIR}")
    if(DEFINED NCBI_EXTERNAL_TREE_ROOT)
        set(_root ${NCBI_EXTERNAL_TREE_ROOT})
    else()
        set(_root ${NCBI_TREE_ROOT})
    endif()

    add_test(NAME ${_test} COMMAND ${CMAKE_COMMAND}
        -DNCBITEST_NAME=${_test}
        -DNCBITEST_ALIAS=${NCBITEST_${_test}_TESTALIAS}
        -DNCBITEST_CONFIG=$<CONFIG>
        -DNCBITEST_COMMAND=${NCBITEST_${_test}_CMD}
        -DNCBITEST_ARGS=${_args}
        -DNCBITEST_TIMEOUT=${_timeout}
        -DNCBITEST_ASSETS=${_assets}
        -DNCBITEST_XOUTDIR=${_xoutdir}
        -DNCBITEST_WATCHER=${_watcher}
        -DNCBITEST_PARAMS=../${NCBI_DIRNAME_TESTING}/TestParams.cmake
        ${_extra}
        -P ${_root}/${NCBITEST_DRIVER}
        WORKING_DIRECTORY .
    )

    NCBI_internal_process_cmake_test_resources(${_test})
    NCBI_internal_process_cmake_test_labels(${_test})
endfunction()

##############################################################################

function(NCBI_internal_AddCMakeTest _variable _access)

    if("${_access}" STREQUAL "MODIFIED_ACCESS" AND DEFINED NCBI_${NCBI_PROJECT}_ALLTESTS)
        foreach(_test IN LISTS NCBI_${NCBI_PROJECT}_ALLTESTS)
            NCBI_internal_add_cmake_test(${_test})
        endforeach()
    endif()

endfunction()

#############################################################################

function(NCBI_internal_FinalizeCMakeTest)

    file(MAKE_DIRECTORY ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_TESTING})
    file(MAKE_DIRECTORY ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}/Testing/Temporary)

    if(DEFINED NCBI_EXTERNAL_TREE_ROOT)
        set(_root ${NCBI_EXTERNAL_TREE_ROOT})
    else()
        set(_root ${NCBI_TREE_ROOT})
    endif()

    # Generate a file with some common test parameters
    # ------------------------------------------------
    
    set(_info "")
   
    # Host / system name / signature / compiler

    cmake_host_system_information(RESULT _host QUERY HOSTNAME)
    string(APPEND _info "set(NCBITEST_HOST        ${_host})\n")
    string(APPEND _info "set(NCBITEST_SYSTEM_NAME ${CMAKE_SYSTEM_NAME})\n")
    string(APPEND _info "set(NCBITEST_OS_NAME     ${HOST_OS})\n")
    string(APPEND _info "set(NCBITEST_OS_DISTR    ${HOST_OS_DISTR})\n")
    string(APPEND _info "set(NCBITEST_COMPILER    ${NCBI_COMPILER})\n")
    string(APPEND _info "set(NCBITEST_SIGNATURE   ${NCBITEST_SIGNATURE})\n")
    string(APPEND _info "\n")

    # Directories
    
    string(APPEND _info "set(NCBITEST_BINDIR ../${NCBI_DIRNAME_RUNTIME})\n")
    string(APPEND _info "set(NCBITEST_LIBDIR ../${NCBI_DIRNAME_ARCHIVE})\n")
    string(APPEND _info "set(NCBITEST_OUTDIR ../${NCBI_DIRNAME_TESTING})\n")
    string(APPEND _info "set(NCBITEST_SOURCEDIR ${NCBI_SRC_ROOT})\n")
    string(APPEND _info "set(NCBITEST_SCRIPTDIR ${_root}/${NCBI_DIRNAME_SCRIPTS})\n")
    string(APPEND _info "\n")
    
    # Project features: Int8GI, Symbols, CfgProps and etc
    
    #list(JOIN NCBI_PTBCFG_PROJECT_FEATURES " " _x)
    string(REPLACE ";" " " _x "${NCBI_PTBCFG_PROJECT_FEATURES}")
    string(APPEND _info "set(NCBITEST_PROJECT_FEATURES ${_x})\n")
    
    # Detected components
    
    #list(JOIN NCBI_ALL_COMPONENTS " " _x)
    string(REPLACE ";" " " _x "${NCBI_ALL_COMPONENTS}")
    string(APPEND _info "set(NCBITEST_COMPONENTS ${_x})\n")
    
    # Build requirements
    
    #list(JOIN NCBI_ALL_REQUIRES " " _x)
    string(REPLACE ";" " " _x "${NCBI_ALL_REQUIRES}")
    string(APPEND _info "set(NCBITEST_REQUIRES ${_x})\n")

    # Combine test features (project_features + components + requires + etc)
    
    set(_features "")
    list(APPEND _features ${CMAKE_SYSTEM_NAME})
    list(APPEND _features ${NCBI_COMPILER})
    list(APPEND _features ${NCBI_PTBCFG_PROJECT_FEATURES})
    list(APPEND _features ${NCBI_ALL_COMPONENTS})
    list(APPEND _features ${NCBI_ALL_REQUIRES})
    list(APPEND _features ${NCBI_ALL_LEGACY})
    #list(JOIN   _features " " _x)
    string(REPLACE ";" " " _x "${_features}")
    string(APPEND _info "set(NCBITEST_FEATURES ${_x})\n")
    string(APPEND _info "\n")

    # Check on linkerd and set backup

    execute_process(
        COMMAND sh -c "echo test | nc -w 1 linkerd 4142"
        RESULT_VARIABLE _retcode
        )
    if (NOT _retcode EQUAL 0)
        string(APPEND _info "set(NCBITEST_LINKERD_BACKUP \"pool.linkerd-proxy.service.bethesda-dev.consul.ncbi.nlm.nih.gov:4142\")\n")
    endif()

    # Check on debug tools to get stack/back trace (except running under memory checkers)

    if(UNIX)
        if(NOT DEFINED NCBITEST_CHECK_TOOL)
            execute_process(
                COMMAND which gdb
                RESULT_VARIABLE _retcode
                OUTPUT_QUIET
                )
            if (_retcode EQUAL 0)
                string(APPEND _info "set(NCBITEST_BACK_TRACE 1)\n")
            endif()
            execute_process(
                COMMAND which gstack
                RESULT_VARIABLE _retcode
                OUTPUT_QUIET
                )
            if (_retcode EQUAL 0)
                string(APPEND _info "set(NCBITEST_STACK_TRACE 1)\n")
            endif()
        endif()
    endif()

    # Check on uptime (for autimated builds)

    set(_uptime 0)
    if(UNIX AND DEFINED $ENV{NCBI_AUTOMATED_BUILD})
        execute_process(
            COMMAND which uptime
            RESULT_VARIABLE _retcode
            )
        if (_retcode EQUAL 0)
            set(_uptime 1)
        endif()
    endif()
    string(APPEND _info "set(NCBITEST_HAVE_UPTIME ${_uptime})\n")
    
    # Create a file:
    file(WRITE ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_TESTING}/TestParams.cmake ${_info})

endfunction()


#############################################################################

NCBI_register_hook(TARGET_ADDED NCBI_internal_AddCMakeTest)
NCBI_register_hook(ALL_ADDED    NCBI_internal_FinalizeCMakeTest)
