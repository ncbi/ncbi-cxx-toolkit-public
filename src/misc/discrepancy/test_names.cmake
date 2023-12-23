
if(NOT ${CMAKE_ARGC} STREQUAL 5)
    message(FATAL_ERROR "Wrong arguments\nUsage:\n${CMAKE_COMMAND} -P ${CMAKE_CURRENT_FUNCTION_LIST_FILE} -- output_file combined_list_of_input_files")
endif()
set(test_names_int "${CMAKE_ARGV3}")
string(REPLACE " " ";" all_cpp_files "${CMAKE_ARGV4}")

function(ParseCPPFile filename codes_var autofix_var)
    file(STRINGS ${filename} _file REGEX "^DISCREPANCY")

    set(all_codes "${${codes_var}}")
    set(all_autofix "${${autofix_var}}")

    foreach(_line ${_file})
        if("${_line}" MATCHES [[^DISCREPANCY_CASE[0-9]*\(]])
            string(REGEX REPLACE [[^DISCREPANCY_CASE[0-9]*\(([A-Za-z0-9_]+).*]] [[\1]] _code "${_line}")
            list(APPEND all_codes ${_code})
        elseif("${_line}" MATCHES [[^DISCREPANCY_AUTOFIX\(]])
            string(REGEX REPLACE [[^DISCREPANCY_AUTOFIX\(([A-Za-z0-9_]+).*]] [[\1]] _autofix "${_line}")
            list(APPEND all_autofix ${_autofix})
        endif()

    endforeach()
    
    set(all_codes   ${all_codes} PARENT_SCOPE)    
    set(all_autofix ${all_autofix} PARENT_SCOPE)
endfunction()

function(ProduceCPPFile )

    list(SORT all_codes)
    list(SORT all_autofix)

    set(_header [[

enum class eTestNames
{
    __CODES_PLACE__,
    max_test_names,
    notset,
};

#define DISC_AUTOFIX_TESTNAMES \
    eTestNames::__AUTOFIX_PLACE__

]]
    )

    list(JOIN all_codes ",\n    " _s_codes)
    list(JOIN all_autofix ",  \\\n    eTestNames::"  _s_autofix)

    string(REPLACE "__CODES_PLACE__" "${_s_codes}" _header "${_header}")
    string(REPLACE "__AUTOFIX_PLACE__" "${_s_autofix}" _header "${_header}")    

    file(WRITE ${test_names_int} "${_header}")
    
endfunction(ProduceCPPFile)

foreach(_filename ${all_cpp_files})
    ParseCPPFile(${_filename} all_codes all_autofix)
endforeach()


ProduceCPPFile()
