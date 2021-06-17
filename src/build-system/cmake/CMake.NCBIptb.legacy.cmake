#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper extension
##  Adds support of ASNTOOL code generation and "TARGET EXPORT"
##    Author: Andrei Gourianov, gouriano@ncbi
##


##############################################################################
# asntool app
if (NOT WIN32)
    set(NCBI_ASNTOOL $ENV{NCBI}/bin/asntool)
    if (IS_DIRECTORY "/am/ncbiapdata/asn")
        set(NCBI_ASNDIR /am/ncbiapdata/asn)
    else()
        set(NCBI_ASNDIR /Volumes/ncbiapdata/asn)
    endif()
else()
    set(NCBI_ASNTOOL //snowman/win-coremake/Lib/Ncbi/C_Toolkit/vs2017.64/c.current/bin/asntool.exe)
endif()

##############################################################################
set(NCBI_GENERATESRC_ASNTOOL   ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}/CMakeFiles/generate_sources.asntool)
if (EXISTS ${NCBI_GENERATESRC_ASNTOOL})
    file(REMOVE ${NCBI_GENERATESRC_ASNTOOL})
endif()

##############################################################################
function(NCBI_generate_by_asntool)
    cmake_parse_arguments(PARSE_ARGV 0 ASNTOOL "LOCAL" "DATASPEC;BASENAME;HEADER" "EXTERNAL;GENFLAGS")

    file(RELATIVE_PATH _rel "${NCBI_SRC_ROOT}" "${NCBI_CURRENT_SOURCE_DIR}")
    set(_inc_dir ${NCBI_INC_ROOT}/${_rel})
    if(IS_ABSOLUTE ${ASNTOOL_DATASPEC})
        set(_specpath ${ASNTOOL_DATASPEC})
    else()
        set(_specpath ${NCBI_CURRENT_SOURCE_DIR}/${ASNTOOL_DATASPEC})
    endif()
    get_filename_component(_specname ${_specpath} NAME_WE)
    get_filename_component(_specext ${_specpath} EXT)
    set(_dataspec ${_specname}${_specext})
    set(_basename ${ASNTOOL_BASENAME})
    if (DEFINED ASNTOOL_HEADER)
        set(_header ${ASNTOOL_HEADER})
    else()
        set(_header ${_specname}.h)
    endif()
    set(_externals "")
    foreach(_module IN LISTS ASNTOOL_EXTERNAL)
        if (NOT "${_externals}" STREQUAL "")
            set(_externals "${_externals},")
        endif()
        if(EXISTS ${NCBI_CURRENT_SOURCE_DIR}/${_module} OR IS_ABSOLUTE ${_module})
            set(_externals "${_externals}${_module}")
        else()
            set(_externals "${_externals}${NCBI_ASNDIR}/${_module}")
        endif()
    endforeach()
    if (NOT "${_externals}" STREQUAL "")
        set(_externals -M ${_externals})
    endif()
    if (ASNTOOL_LOCAL)
        add_custom_command(
            OUTPUT  ${NCBI_CURRENT_SOURCE_DIR}/${_basename}.c ${NCBI_CURRENT_SOURCE_DIR}/${_basename}.h ${NCBI_CURRENT_SOURCE_DIR}/${_header}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_specpath} ${NCBI_CURRENT_SOURCE_DIR}
            COMMAND ${NCBI_ASNTOOL} -w 100 -m ${_dataspec} -o ${_header}
            COMMAND ${NCBI_ASNTOOL} -w 100 -G -m ${_dataspec} -B ${_basename} ${_externals} -K ${_header} ${ASNTOOL_GENFLAGS}
            WORKING_DIRECTORY ${NCBI_CURRENT_SOURCE_DIR}
            COMMENT "Generate C object loader from ${_dataspec}"
            DEPENDS ${_specpath}
        )

        string(REPLACE ";" " " _externals "${_externals}")
        string(REPLACE ";" " " ASNTOOL_GENFLAGS "${ASNTOOL_GENFLAGS}")
        file(APPEND ${NCBI_GENERATESRC_ASNTOOL} "cd ${NCBI_CURRENT_SOURCE_DIR}\n")
        file(APPEND ${NCBI_GENERATESRC_ASNTOOL} "${CMAKE_COMMAND} -E copy_if_different ${_specpath} ${NCBI_CURRENT_SOURCE_DIR}\n")
        file(APPEND ${NCBI_GENERATESRC_ASNTOOL} "${NCBI_ASNTOOL} -w 100 -m ${_dataspec} -o ${_header}\n")
        file(APPEND ${NCBI_GENERATESRC_ASNTOOL} "${NCBI_ASNTOOL} -w 100 -G -m ${_dataspec} -B ${_basename} ${_externals} -K ${_header} ${ASNTOOL_GENFLAGS}\n")
    else()
        add_custom_command(
            OUTPUT  ${NCBI_CURRENT_SOURCE_DIR}/${_basename}.c ${_inc_dir}/${_basename}.h ${_inc_dir}/${_header}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${_inc_dir}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_specpath} ${NCBI_CURRENT_SOURCE_DIR}
            COMMAND ${NCBI_ASNTOOL} -w 100 -m ${_dataspec} -o ${_inc_dir}/${_header}
            COMMAND ${NCBI_ASNTOOL} -w 100 -G -m ${_dataspec} -B ${_basename} ${_externals} -K ${_header} ${ASNTOOL_GENFLAGS}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_basename}.h ${_inc_dir}
            COMMAND ${CMAKE_COMMAND} -E remove -f ${_basename}.h
            WORKING_DIRECTORY ${NCBI_CURRENT_SOURCE_DIR}
            COMMENT "Generate C object loader from ${_dataspec}"
            DEPENDS ${_specpath}
        )

        string(REPLACE ";" " " _externals "${_externals}")
        string(REPLACE ";" " " ASNTOOL_GENFLAGS "${ASNTOOL_GENFLAGS}")
        file(APPEND ${NCBI_GENERATESRC_ASNTOOL} "cd ${NCBI_CURRENT_SOURCE_DIR}\n")
        file(APPEND ${NCBI_GENERATESRC_ASNTOOL} "${CMAKE_COMMAND} -E make_directory ${_inc_dir}\n")
        file(APPEND ${NCBI_GENERATESRC_ASNTOOL} "${CMAKE_COMMAND} -E copy_if_different ${_specpath} ${NCBI_CURRENT_SOURCE_DIR}\n")
        file(APPEND ${NCBI_GENERATESRC_ASNTOOL} "${NCBI_ASNTOOL} -w 100 -m ${_dataspec} -o ${_inc_dir}/${_header}\n")
        file(APPEND ${NCBI_GENERATESRC_ASNTOOL} "${NCBI_ASNTOOL} -w 100 -G -m ${_dataspec} -B ${_basename} ${_externals} -K ${_header} ${ASNTOOL_GENFLAGS}\n")
        file(APPEND ${NCBI_GENERATESRC_ASNTOOL} "${CMAKE_COMMAND} -E copy_if_different ${_basename}.h ${_inc_dir}\n")
        file(APPEND ${NCBI_GENERATESRC_ASNTOOL} "${CMAKE_COMMAND} -E remove -f ${_basename}.h\n")
    endif()
endfunction()

##############################################################################
function(NCBI_target_export)
    cmake_parse_arguments(PARSE_ARGV 0 EXPORT "" "TARGET;DESTINATION" "HEADERS")
    if(WIN32 OR XCODE)
        set(_dest ${NCBI_CFGINC_ROOT}/$<CONFIG>/${EXPORT_DESTINATION})
    else()
        set(_dest ${NCBI_CFGINC_ROOT}/${EXPORT_DESTINATION})
    endif()
    file(RELATIVE_PATH _rel "${NCBI_SRC_ROOT}" "${NCBI_CURRENT_SOURCE_DIR}")
    set(_inc_dir ${NCBI_INC_ROOT}/${_rel})
if(OFF)
    foreach(_header IN LISTS EXPORT_HEADERS)
        if (EXISTS "${NCBI_CURRENT_SOURCE_DIR}/${_header}")
            set(_headers ${_headers} ${_header})
        else()
            set(_headers ${_headers} ${_inc_dir}/${_header})
        endif()
    endforeach()
else()
    set(_headers ${_headers} ${EXPORT_HEADERS})
endif()
    add_custom_command(
        TARGET ${EXPORT_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${_dest}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_headers} ${_dest}
        WORKING_DIRECTORY ${NCBI_CURRENT_SOURCE_DIR}
        COMMENT "Exporting ${EXPORT_TARGET} headers into ${NCBI_CFGINC_ROOT}"
    )
endfunction()
