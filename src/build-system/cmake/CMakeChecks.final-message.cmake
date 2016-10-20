
#
# Config to dump diagnostic information at the completion of configuration
#

get_directory_property(Defs COMPILE_DEFINITIONS)
foreach (d ${Defs} )
    set(DefsStr "${DefsStr} -D${d}")
endforeach()

message("------------------------------------------------------------------------------")
message("Build Target:   ${CMAKE_BUILD_TYPE}")
message("Shared Libs:    ${BUILD_SHARED_LIBS}")
message("Top Source Dir: ${top_src_dir}")
message("Build Root:     ${build_root}")
message("Executable Dir: ${EXECUTABLE_OUTPUT_PATH}")
message("Library Dir:    ${LIBRARY_OUTPUT_PATH}")
message("C Compiler:     ${CMAKE_C_COMPILER}")
message("C++ Compiler:   ${CMAKE_CXX_COMPILER}")
message("CFLAGS:         ${CMAKE_C_FLAGS}")
message("CXXFLAGS:       ${CMAKE_CXX_FLAGS}")
message("Compile Flags:  ${DefsStr}")
message("PCRE:           ${PCRE_LIBRARIES}")
message("ZLIB:           ${ZLIB_LIBRARIES}")
message("BZip2:          ${BZIP2_LIBRARIES}")
message("LZO:            ${LZO_LIBRARIES}")
message("Boost:          ${Boost_INCLUDE_DIRS}")
message("wxWidgets:")
message("  include:      ${wxWidgets_INCLUDE_DIRS}")
message("  lib:          ${wxWidgets_LIBRARIES}")
message("  definitions:  ${wxWidgets_DEFNITIONS}")
message("  flags:        ${wxWidgets_CXX_FLAGS}")

message("------------------------------------------------------------------------------")
