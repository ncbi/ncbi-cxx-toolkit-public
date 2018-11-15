
#
# Config to dump diagnostic information at the completion of configuration
#

get_directory_property(Defs COMPILE_DEFINITIONS)
foreach (d ${Defs} )
    set(DefsStr "${DefsStr} -D${d}")
endforeach()

if ( NOT "${NCBI_MODULES_FOUND}" STREQUAL "")
    list(REMOVE_DUPLICATES NCBI_MODULES_FOUND)
endif()
foreach (mod ${NCBI_MODULES_FOUND})
    set(MOD_STR "${MOD_STR} ${mod}")
endforeach()

#STRING(SUBSTRING "${EXTERNAL_LIBRARIES_COMMENT}" 1 -1 EXTERNAL_LIBRARIES_COMMENT)

function(ShowMainBoilerplate)
    message("")
    message("------------------------------------------------------------------------------")
    message("CMake exe:      ${CMAKE_COMMAND}")
    message("CMake version:  ${CMAKE_VERSION}")
    message("Build Target:   ${CMAKE_BUILD_TYPE}")
    message("Shared Libs:    ${BUILD_SHARED_LIBS}")
    message("Top Source Dir: ${top_src_dir}")
    message("Build Root:     ${build_root}")
    if (NCBI_EXPERIMENTAL_CFG)
        message("Executable Dir: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        message("Archive Dir:    ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
        message("Library Dir:    ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    else()
        message("Executable Dir: ${EXECUTABLE_OUTPUT_PATH}")
        message("Library Dir:    ${LIBRARY_OUTPUT_PATH}")
    endif()
    message("C Compiler:     ${CMAKE_C_COMPILER}")
    message("C++ Compiler:   ${CMAKE_CXX_COMPILER}")
    if (CMAKE_USE_DISTCC AND DISTCC_EXECUTABLE)
        message("    distcc:     ${DISTCC_EXECUTABLE}")
    endif()
    if (CMAKE_USE_CCACHE AND CCACHE_EXECUTABLE)
        message("    ccache:     ${CCACHE_EXECUTABLE}")
    endif()
    message("CFLAGS:        ${CMAKE_C_FLAGS}")
    message("CXXFLAGS:      ${CMAKE_CXX_FLAGS}")
    message("Compile Flags: ${DefsStr}")
    message("DataTool Ver:   ${_datatool_version}")
    message("DataTool Path:  ${NCBI_DATATOOL}")
    message("")
    if (NCBI_EXPERIMENTAL_CFG)
        message("Components Found:  ${NCBI_ALL_COMPONENTS}")
    else()
        message("Modules Found:  ${MOD_STR}")
    endif()

    message("------------------------------------------------------------------------------")
    message("")
endfunction()

ShowMainBoilerplate()

if (NOT NCBI_EXPERIMENTAL_CFG)
add_custom_target(show-config
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
endif()
