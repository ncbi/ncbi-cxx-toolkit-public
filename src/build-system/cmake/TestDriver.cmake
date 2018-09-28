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
            message(SEND_ERROR "Test ${NCBITEST_NAME} ERROR: asset ${NCBITEST_SOURCEDIR}/${_res} not found")
            return()
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
    file(COPY ${NCBITEST_SOURCEDIR}/${_res} DESTINATION ${_workdir})
endforeach()

if(WIN32)
    string(REPLACE "/" "\\" NCBITEST_BINDIR  ${NCBITEST_BINDIR})
    set(ENV{PATH} "${NCBITEST_BINDIR}\\${NCBITEST_CONFIG};$ENV{PATH}")
    set(_testdata //snowman/win-coremake/scripts/test_data)
elseif(XCODE)
    set(ENV{PATH} ".:${NCBITEST_BINDIR}/${NCBITEST_CONFIG}:$ENV{PATH}")
    set(ENV{CHECK_EXEC} "time")
    set(_testdata /am/ncbiapdata/test_data)
else()
    set(ENV{PATH} ".:${NCBITEST_BINDIR}:$ENV{PATH}")
    set(ENV{CHECK_EXEC} "time")
    set(_testdata /am/ncbiapdata/test_data)
endif()

if(EXISTS "${_testdata}")
    set(ENV{NCBI_TEST_DATA} "${_testdata}")
endif()

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
    message(SEND_ERROR "Test ${NCBITEST_NAME} failed")
endif()
