#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI C++ Toolkit Conan package code generation helper
##    Author: Andrei Gourianov, gouriano@ncbi
##


##############################################################################
# Find datatool app
set(NCBI_DATATOOL "${CMAKE_CURRENT_LIST_DIR}/../../../bin/datatool${CMAKE_EXECUTABLE_SUFFIX}")

##############################################################################
function(NCBI_generate_cpp GEN_SOURCES GEN_HEADERS)
    if ("${ARGC}" LESS "3")
        message(FATAL_ERROR "NCBI_generate_cpp: no dataspecs provided")
    endif()
    set(_dt_specs .asn;.dtd;.xsd;.wsdl;.jsd;.json)

    set(_all_srcfiles)
    set(_all_incfiles)
    foreach(_spec IN LISTS ARGN)
        set(_DATASPEC ${_spec})
        if(NOT IS_ABSOLUTE ${_DATASPEC})
            set(_DATASPEC ${CMAKE_CURRENT_SOURCE_DIR}/${_DATASPEC})
            get_filename_component(_path     ${_DATASPEC} DIRECTORY)
            get_filename_component(_basename ${_DATASPEC} NAME_WE)
            get_filename_component(_ext      ${_DATASPEC} EXT)
        endif()
        if (NOT EXISTS "${_DATASPEC}")
            message(FATAL_ERROR "${_DATASPEC}: File not found")
        endif()
        if("${_ext}" IN_LIST _dt_specs)
            if (NOT EXISTS "${NCBI_DATATOOL}")
                message(FATAL_ERROR "Datatool code generator not found")
            endif()

            set(_this_specfile   ${_DATASPEC})
            set(_this_srcfiles   ${_path}/${_basename}__.cpp ${_path}/${_basename}___.cpp)
            set(_this_incfiles   ${_path}/${_basename}__.hpp)
            list(APPEND _all_srcfiles ${_this_srcfiles})
            list(APPEND _all_incfiles ${_this_incfiles})

            set(_oc ${_basename})
            set(_od ${_path}/${_basename}.def)
            set(_depends ${NCBI_DATATOOL} ${_this_specfile})
            if(EXISTS ${_od})
                set(_depends ${_depends} ${_od})
            endif()
            set(_cmd ${NCBI_DATATOOL} -m ${_this_specfile} -oA -oc ${_oc} -oph . -opc . -orq -od ${_od} -odi)
            set_source_files_properties(${_this_srcfiles} ${_this_incfiles} PROPERTIES GENERATED TRUE)
            add_custom_command(
                OUTPUT ${_this_srcfiles} ${_this_incfiles} ${_path}/${_basename}.files
                COMMAND ${_cmd} VERBATIM
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMENT "Generate C++ classes from ${_this_specfile}"
                DEPENDS ${_depends}
                VERBATIM
            )
        else()
            message(FATAL_ERROR "NCBI_generate_cpp: unsupported specification: ${_spec}")
        endif()
    endforeach()
    set(${GEN_SOURCES} ${_all_srcfiles}  PARENT_SCOPE)
    set(${GEN_HEADERS} ${_all_incfiles}  PARENT_SCOPE)
endfunction()
