
#
# Config to dump diagnostic information at the completion of configuration
#

get_directory_property(Defs COMPILE_DEFINITIONS)
foreach (d ${Defs} )
    set(DefsStr "${DefsStr} -D${d}")
endforeach()

STRING(SUBSTRING "${EXTERNAL_LIBRARIES_COMMENT}" 1 -1 EXTERNAL_LIBRARIES_COMMENT)

function(ShowMainBoilerplate)
    message("")
    message("------------------------------------------------------------------------------")
    message("Build Target:   ${CMAKE_BUILD_TYPE}")
    message("Shared Libs:    ${BUILD_SHARED_LIBS}")
    message("Top Source Dir: ${top_src_dir}")
    message("Build Root:     ${build_root}")
    message("Executable Dir: ${EXECUTABLE_OUTPUT_PATH}")
    message("Library Dir:    ${LIBRARY_OUTPUT_PATH}")
    if ("${CMAKE_C_COMPILER_ARG1}" STREQUAL "")
        message("C Compiler:     ${CMAKE_C_COMPILER}")
    else()
        message("C Compiler:     ${CMAKE_C_COMPILER_ARG1}")
        message("   Wrapper:     ${CMAKE_C_COMPILER}")
    endif()
    if ("${CMAKE_CXX_COMPILER_ARG1}" STREQUAL "")
        message("C++ Compiler:   ${CMAKE_CXX_COMPILER}")
    else()
        message("C++ Compiler:   ${CMAKE_C_COMPILER_ARG1}")
        message("     Wrapper:   ${CMAKE_CXX_COMPILER}")
    endif()
    message("CFLAGS:        ${CMAKE_C_FLAGS}")
    message("CXXFLAGS:      ${CMAKE_CXX_FLAGS}")
    message("Compile Flags: ${DefsStr}")
    message("")
    message("Modules Found: ${TOOLKIT_MODULES_FOUND}")

    message("------------------------------------------------------------------------------")
    message("")
endfunction()

ShowMainBoilerplate()

add_custom_target(NAME show-config
    COMMAND ${CMAKE_COMMAND} -e echo "ShowMainBoilerplate()"
    COMMAND ${CMAKE_COMMAND} -e echo "PCRE:           ${PCRE_LIBRARIES}"
    COMMAND ${CMAKE_COMMAND} -e echo "Boost:          ${Boost_INCLUDE_DIRS}"
    COMMAND ${CMAKE_COMMAND} -e echo "wxWidgets:"
    COMMAND ${CMAKE_COMMAND} -e echo "  include:      ${wxWidgets_INCLUDE_DIRS}"
    COMMAND ${CMAKE_COMMAND} -e echo "  lib:          ${wxWidgets_LIBRARIES}"
    COMMAND ${CMAKE_COMMAND} -e echo "  definitions:  ${wxWidgets_DEFNITIONS}"
    COMMAND ${CMAKE_COMMAND} -e echo "  flags:        ${wxWidgets_CXX_FLAGS}"
    COMMAND ${CMAKE_COMMAND} -e echo "GnuTLS:         ${GNUTLS_COMMENT}"
    COMMAND ${CMAKE_COMMAND} -e echo "GnuTLS include: ${GNUTLS_INCLUDE}"
    COMMAND ${CMAKE_COMMAND} -e echo "${EXTERNAL_LIBRARIES_COMMENT}"
    )


