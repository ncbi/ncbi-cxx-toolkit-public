#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper extension
##  In NCBI CMake wrapper, adds CMake tests (which use CMake testing framework)
##    Author: Andrei Gourianov, gouriano@ncbi
##


##############################################################################
macro(NCBI_internal_process_cmake_test_requires _test)
    set(NCBITEST_REQUIRE_NOTFOUND "")
    set(_all ${NCBITEST__REQUIRES} ${NCBITEST_${_test}_REQUIRES})
    if (NOT "${_all}" STREQUAL "")
        list(REMOVE_DUPLICATES _all)
    endif()

    foreach(_req IN LISTS _all)
        NCBI_util_parse_sign( ${_req} _value _negate)
        if (NCBI_REQUIRE_${_value}_FOUND OR NCBI_COMPONENT_${_value}_FOUND)
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
function(NCBI_internal_add_cmake_test _test)
    if( NOT DEFINED NCBITEST_${_test}_CMD)
        set(NCBITEST_${_test}_CMD ${NCBI_${NCBI_PROJECT}_OUTPUT})
    endif()
    get_filename_component(_ext ${NCBITEST_${_test}_CMD} EXT)
    if("${_ext}" STREQUAL ".sh")
        set(NCBITEST_${_test}_REQUIRES ${NCBITEST_${_test}_REQUIRES} -MSWin)
        set(NCBITEST_${_test}_ASSETS   ${NCBITEST_${_test}_ASSETS}   ${NCBITEST_${_test}_CMD})
    endif()
    set(_assets ${NCBITEST__ASSETS} ${NCBITEST_${_test}_ASSETS})
    if (DEFINED NCBITEST_${_test}_TIMEOUT)
        set(_timeout ${NCBITEST_${_test}_TIMEOUT})
    elseif(DEFINED NCBITEST__TIMEOUT)
        set(_timeout ${NCBITEST__TIMEOUT})
    else()
        set(_timeout 86400)
    endif()
    string(REPLACE ";" " " _args    "${NCBITEST_${_test}_ARG}")
    string(REPLACE ";" " " _assets   "${_assets}")

    if (XCODE)
        set(_extra -DXCODE=TRUE)
    endif()

    file(RELATIVE_PATH _xoutdir "${NCBI_SRC_ROOT}" "${NCBI_CURRENT_SOURCE_DIR}")
    if (WIN32 OR XCODE)
        set(_outdir ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_TESTING}/$<CONFIG>/${_xoutdir})
    else()
        set(_outdir ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_TESTING}/${_xoutdir})
    endif()

    NCBI_internal_process_cmake_test_requires(${_test})
    if ( NOT "${NCBITEST_REQUIRE_NOTFOUND}" STREQUAL "")
        if(NCBI_VERBOSE_ALLPROJECTS OR NCBI_VERBOSE_PROJECT_${NCBI_PROJECT})
            message("${NCBI_PROJECT} (${NCBI_CURRENT_SOURCE_DIR}): Test ${_test} of project ${NCBI_PROJECT} is excluded because of unmet requirements: ${NCBITEST_REQUIRE_NOTFOUND}")
        endif()
        return()
    endif()

    set(_auto $ENV{NCBI_AUTOMATED_BUILD})
    if(_auto)
    add_test(NAME ${_test} COMMAND ${CMAKE_COMMAND}
        -DNCBITEST_NAME=${_test}
        -DNCBITEST_CONFIG=$<CONFIG>
        -DNCBITEST_COMMAND=${NCBITEST_${_test}_CMD}
        -DNCBITEST_ARGS=${_args}
        -DNCBITEST_TIMEOUT=${_timeout}
        -DNCBITEST_BINDIR=../${NCBI_DIRNAME_RUNTIME}
        -DNCBITEST_SOURCEDIR=../../${NCBI_DIRNAME_SRC}/${_xoutdir}
        -DNCBITEST_OUTDIR=../${NCBI_DIRNAME_TESTING}/${_xoutdir}
        -DNCBITEST_ASSETS=${_assets}
        ${_extra}
        -P "../../${NCBI_DIRNAME_CMAKECFG}/TestDriver.cmake"
        WORKING_DIRECTORY .
    )
    else()
    add_test(NAME ${_test} COMMAND ${CMAKE_COMMAND}
        -DNCBITEST_NAME=${_test}
        -DNCBITEST_CONFIG=$<CONFIG>
        -DNCBITEST_COMMAND=${NCBITEST_${_test}_CMD}
        -DNCBITEST_ARGS=${_args}
        -DNCBITEST_TIMEOUT=${_timeout}
        -DNCBITEST_BINDIR=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        -DNCBITEST_SOURCEDIR=${NCBI_CURRENT_SOURCE_DIR}
        -DNCBITEST_OUTDIR=${_outdir}
        -DNCBITEST_ASSETS=${_assets}
        ${_extra}
        -P "${NCBITEST_DRIVER}")
    endif()
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
NCBI_register_hook(TARGET_ADDED NCBI_internal_AddCMakeTest)
