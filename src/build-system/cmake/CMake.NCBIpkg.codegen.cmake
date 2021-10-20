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
if(NOT DEFINED NCBITK_TREE_ROOT)
    set(NCBI_PKG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
else()
    set(NCBI_PKG_ROOT "${NCBITK_TREE_ROOT}")
endif()
if(NOT DEFINED NCBI_DATATOOL)
    set(NCBI_DATATOOL "${NCBI_PKG_ROOT}/bin/datatool${CMAKE_EXECUTABLE_SUFFIX}")
endif()
if(NOT DEFINED NCBI_PROTOC_APP)
    set(NCBI_PROTOC_APP "${CONAN_BIN_DIRS_PROTOBUF}/protoc${CMAKE_EXECUTABLE_SUFFIX}")
endif()
if(NOT DEFINED NCBI_GRPC_PLUGIN)
    set(NCBI_GRPC_PLUGIN "${CONAN_BIN_DIRS_GRPC}/grpc_cpp_plugin${CMAKE_EXECUTABLE_SUFFIX}")
endif()

##############################################################################
macro(NCBI_generate_cpp)
    set(__ncbi_add_dotinc)
    NCBI_internal_generate_cpp(${ARGV})
    if(NOT "${__ncbi_add_dotinc}" STREQUAL "")
        include_directories(${__ncbi_add_dotinc})
    endif()
endmacro()

# GEN_OPTIONS:
#   for .proto specs:
#       proto   - generate PROTOBUF code (default = ON) 
#       -proto  - do not generate PROTOBUF code
#       grpc    - generate GRPC code (default = OFF) 
#       -grpc   - do not generate GRPC code
#       mock    - when grpc is ON, generate MOCK code (default = OFF) 
#       -mock   - do not generate MOCK code
#
##############################################################################
function(NCBI_internal_generate_cpp GEN_SOURCES GEN_HEADERS)
    if ("${ARGC}" LESS "3")
        message(FATAL_ERROR "NCBI_generate_cpp: no dataspecs provided")
    endif()
    cmake_parse_arguments(PARSE_ARGV 2 GEN "" "" "GEN_OPTIONS")
    set(_dt_specs .asn;.dtd;.xsd;.wsdl;.jsd;.json)
    set(_protoc_specs .proto)

    set(_all_srcfiles)
    set(_all_incfiles)
    foreach(_spec IN LISTS GEN_UNPARSED_ARGUMENTS)
        set(_DATASPEC ${_spec})
        if(NOT IS_ABSOLUTE ${_DATASPEC})
            set(_DATASPEC ${CMAKE_CURRENT_SOURCE_DIR}/${_DATASPEC})
        endif()
        get_filename_component(_path     ${_DATASPEC} DIRECTORY)
        get_filename_component(_basename ${_DATASPEC} NAME_WE)
        get_filename_component(_ext      ${_DATASPEC} EXT)
        file(RELATIVE_PATH     _relpath  ${CMAKE_CURRENT_SOURCE_DIR} ${_path})
        if("${_relpath}" STREQUAL "")
            set(_relpath .)
        endif()
        if (NOT EXISTS "${_DATASPEC}")
            message(FATAL_ERROR "${_DATASPEC}: File not found")
        endif()
        set(_this_specfile   ${_DATASPEC})

        if("${_ext}" IN_LIST _dt_specs)
            if (NOT EXISTS "${NCBI_DATATOOL}")
                message(FATAL_ERROR "Datatool code generator not found")
            endif()

            set(_this_srcfiles   ${_path}/${_basename}__.cpp ${_path}/${_basename}___.cpp)
            set(_this_incfiles   ${_path}/${_basename}__.hpp)
            list(APPEND _all_srcfiles ${_this_srcfiles})
            list(APPEND _all_incfiles ${_this_incfiles})

            set(_module_imports "")
            set(_imports "")
            if(EXISTS "${_path}/${_basename}.module")
                FILE(READ "${_path}/${_basename}.module" _module_contents)
                STRING(REGEX MATCH "MODULE_IMPORT *=[^\n]*[^ \n]" _tmp "${_module_contents}")
                STRING(REGEX REPLACE "MODULE_IMPORT *= *" "" _tmp "${_tmp}")
                STRING(REGEX REPLACE "  *$" "" _imp_list "${_tmp}")
                STRING(REGEX REPLACE " " ";" _imp_list "${_imp_list}")

                foreach(_module IN LISTS _imp_list)
                    set(_module_imports "${_module_imports} ${_module}${_ext}")
                endforeach()
                if (NOT "${_module_imports}" STREQUAL "")
                    set(_imports -ors -opm "${NCBI_PKG_ROOT}/res/specs" -M ${_module_imports})
                endif()
            endif()
            set(_od ${_path}/${_basename}.def)
            set(_depends ${NCBI_DATATOOL} ${_this_specfile})
            if(EXISTS ${_od})
                set(_depends ${_depends} ${_od})
            endif()
            set(_cmd ${NCBI_DATATOOL} -m ${_this_specfile} -oA -oc ${_basename} -oph ${_relpath} -opc ${_relpath} -od ${_od} -odi ${_imports})
#message("_cmd = ${_cmd}")
            set_source_files_properties(${_this_srcfiles} ${_this_incfiles} PROPERTIES GENERATED TRUE)
            add_custom_command(
                OUTPUT ${_this_srcfiles} ${_this_incfiles} ${_path}/${_basename}.files
                COMMAND ${_cmd} VERBATIM
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMENT "Generate C++ classes from ${_this_specfile}"
                DEPENDS ${_depends}
                VERBATIM
            )
            list(APPEND __ncbi_add_dotinc ${_path})
        elseif("${_ext}" IN_LIST _protoc_specs)
            if(NOT "-proto" IN_LIST GEN_GEN_OPTIONS)
                if (NOT EXISTS "${NCBI_PROTOC_APP}")
                    message(FATAL_ERROR "Protoc code generator not found")
                endif()

                set(_this_srcfiles   ${_path}/${_basename}.pb.cc)
                set(_this_incfiles   ${_path}/${_basename}.pb.h)
                list(APPEND _all_srcfiles ${_this_srcfiles})
                list(APPEND _all_incfiles ${_this_incfiles})

                set(_depends ${NCBI_PROTOC_APP} ${_this_specfile})
                set(_cmd ${NCBI_PROTOC_APP} --cpp_out=${_relpath} -I${_relpath} ${_basename}${_ext})
                add_custom_command(
                    OUTPUT ${_this_srcfiles} ${_this_incfiles}
                    COMMAND ${_cmd} VERBATIM
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    COMMENT "Generate PROTOC C++ classes from ${_this_specfile}"
                    DEPENDS ${_depends}
                    VERBATIM
                )
            endif()

            if("grpc" IN_LIST GEN_GEN_OPTIONS)
                if (NOT EXISTS "${NCBI_GRPC_PLUGIN}")
                    message(FATAL_ERROR "GRPC CPP plugin not found")
                endif()

                set(_this_srcfiles   ${_path}/${_basename}.grpc.pb.cc)
                set(_this_incfiles   ${_path}/${_basename}.grpc.pb.h)
                list(APPEND _all_srcfiles ${_this_srcfiles})
                list(APPEND _all_incfiles ${_this_incfiles})

                set(_depends ${NCBI_PROTOC_APP} ${NCBI_GRPC_PLUGIN} ${_this_specfile})
                if("mock" IN_LIST GEN_GEN_OPTIONS)
                    set(_cmd ${NCBI_PROTOC_APP} --grpc_out=generate_mock_code=true:${_relpath} --plugin=protoc-gen-grpc=${NCBI_GRPC_PLUGIN} -I${_relpath} ${_basename}${_ext})
                else()
                    set(_cmd ${NCBI_PROTOC_APP} --grpc_out=${_relpath} --plugin=protoc-gen-grpc=${NCBI_GRPC_PLUGIN} -I${_relpath} ${_basename}${_ext})
                endif()
                add_custom_command(
                    OUTPUT ${_this_srcfiles} ${_this_incfiles}
                    COMMAND ${_cmd} VERBATIM
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    COMMENT "Generate GRPC C++ classes from ${_this_specfile}"
                    DEPENDS ${_depends}
                    VERBATIM
                )
            endif()
        else()
            message(FATAL_ERROR "NCBI_generate_cpp: unsupported specification: ${_spec}")
        endif()
    endforeach()
    if(NOT "${__ncbi_add_dotinc}" STREQUAL "")
        list(REMOVE_DUPLICATES __ncbi_add_dotinc)
        set(__ncbi_add_dotinc ${__ncbi_add_dotinc} PARENT_SCOPE)
    endif()
    set(${GEN_SOURCES} ${_all_srcfiles}  PARENT_SCOPE)
    set(${GEN_HEADERS} ${_all_incfiles}  PARENT_SCOPE)
endfunction()
