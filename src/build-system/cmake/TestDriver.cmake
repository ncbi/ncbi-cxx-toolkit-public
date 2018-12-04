#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper: Test driver
##    Author: Andrei Gourianov, gouriano@ncbi
##


string(REPLACE " " ";" NCBITEST_ARGS    "${NCBITEST_ARGS}")
string(REPLACE " " ";" NCBITEST_ASSETS  "${NCBITEST_ASSETS}")

if (NOT "${NCBITEST_ASSETS}" STREQUAL "")
    list(REMOVE_DUPLICATES NCBITEST_ASSETS)
    foreach(_res IN LISTS NCBITEST_ASSETS)
        if (NOT EXISTS ${NCBITEST_SOURCEDIR}/${_res})
            message(WARNING "Test ${NCBITEST_NAME} WARNING: test asset ${NCBITEST_SOURCEDIR}/${_res} not found")
        endif()
    endforeach()
endif()

string(RANDOM _subdir)
set(_subdir ${NCBITEST_NAME}_${_subdir})

file(MAKE_DIRECTORY ${NCBITEST_OUTDIR}/${_subdir})
set(_workdir ${NCBITEST_OUTDIR}/${_subdir})
set(_output ${NCBITEST_OUTDIR}/${NCBITEST_NAME}.output.txt)
if(EXISTS ${_output})
    file(REMOVE ${_output})
endif()

foreach(_res IN LISTS NCBITEST_ASSETS)
    if (EXISTS ${NCBITEST_SOURCEDIR}/${_res})
        file(COPY ${NCBITEST_SOURCEDIR}/${_res} DESTINATION ${_workdir})
    endif()
endforeach()

if(WIN32)
    set(_scripts  "\\\\snowman\\win-coremake\\Scripts\\internal_scripts\\cpp_common\\impl")
    set(_testdata "\\\\snowman\\win-coremake\\scripts\\test_data")
    string(REPLACE "/" "\\" _cfg_bin  "${NCBITEST_BINDIR}")
    set(_cfg_lib "${NCBITEST_BINDIR}\\..\\lib")
    if(EXISTS "${_scripts}")
        set(_scripts "${_scripts};")
    else()
        unset(_scripts)
    endif()
else()
    set(_scripts  "/am/ncbiapdata/scripts/cpp_common/impl")
    set(_testdata "/am/ncbiapdata/test_data")
    set(_cfg_bin "${NCBITEST_BINDIR}")
    set(_cfg_lib "${NCBITEST_BINDIR}/../lib")
    if(EXISTS "${_scripts}")
        set(_scripts "${_scripts}:")
    else()
        unset(_scripts)
    endif()
endif()

if(EXISTS "${_testdata}")
    set(ENV{NCBI_TEST_DATA} "${_testdata}")
endif()

if(WIN32)
    set(ENV{PATH}    "${_cfg_bin}\\${NCBITEST_CONFIG};${_cfg_lib}\\${NCBITEST_CONFIG};${_scripts}$ENV{PATH}")
    set(ENV{CFG_BIN} "${_cfg_bin}\\${NCBITEST_CONFIG}")
    set(ENV{CFG_LIB} "${_cfg_lib}\\${NCBITEST_CONFIG}")
elseif(XCODE)
    set(ENV{PATH}  ".:${_cfg_bin}/${NCBITEST_CONFIG}:${_cfg_lib}/${NCBITEST_CONFIG}:${_scripts}$ENV{PATH}")
    set(ENV{CFG_BIN} "${_cfg_bin}/${NCBITEST_CONFIG}")
    set(ENV{CFG_LIB} "${_cfg_lib}/${NCBITEST_CONFIG}")
#    set(ENV{CHECK_EXEC} "time")
else()
    set(ENV{PATH}  ".:${_cfg_bin}:${_cfg_lib}:${_scripts}$ENV{PATH}")
    set(ENV{CFG_BIN} "${_cfg_bin}")
    set(ENV{CFG_LIB} "${_cfg_lib}")
#    set(ENV{CHECK_EXEC} "time")
endif()
set(ENV{CHECK_EXEC} " ")

set(_result "1")
execute_process(
    COMMAND           ${NCBITEST_COMMAND} ${NCBITEST_ARGS}
    WORKING_DIRECTORY ${_workdir}
    TIMEOUT           ${NCBITEST_TIMEOUT}
    RESULT_VARIABLE   _result
    OUTPUT_FILE       ${_output}
    ERROR_FILE        ${_output}
)
file(REMOVE_RECURSE ${_workdir})

if (NOT ${_result} EQUAL "0")
    message(SEND_ERROR "Test ${NCBITEST_NAME} failed (error=${_result})")
endif()
