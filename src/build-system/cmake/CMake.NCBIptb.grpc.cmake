#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper extension
##  In NCBI CMake wrapper, adds support of GRPC/PROTOBUF code generation
##    Author: Andrei Gourianov, gouriano@ncbi
##


##############################################################################
function(NCBI_internal_process_proto_dataspec _variable _access _value)
    if(NOT "${_access}" STREQUAL "MODIFIED_ACCESS" OR "${_value}" STREQUAL "")
        return()
    endif()
    cmake_parse_arguments(DT "GENERATE" "DATASPEC;RETURN" "REQUIRES" ${_value})

    get_filename_component(_path     ${DT_DATASPEC} DIRECTORY)
    get_filename_component(_basename ${DT_DATASPEC} NAME_WE)
    get_filename_component(_ext      ${DT_DATASPEC} EXT)
    file(RELATIVE_PATH     _relpath  ${NCBI_SRC_ROOT} ${_path})

    set(_specfiles  ${DT_DATASPEC})
    set(_pb_srcfiles "")
    set(_pb_incfiles "")
    if ("PROTOBUF" IN_LIST DT_REQUIRES)
        set(_pb_srcfiles ${_path}/${_basename}.pb.cc)
        set(_pb_incfiles ${NCBI_INC_ROOT}/${_relpath}/${_basename}.pb.h)
    endif()
    set(_gr_srcfiles "")
    set(_gr_incfiles "")
    if ("GRPC" IN_LIST DT_REQUIRES)
        set(_gr_srcfiles ${_path}/${_basename}.grpc.pb.cc)
        set(_gr_incfiles ${NCBI_INC_ROOT}/${_relpath}/${_basename}.grpc.pb.h)
    endif()
    if(NOT "${DT_RETURN}" STREQUAL "")
#        set(${DT_RETURN} DATASPEC ${_specfiles} SOURCES ${_pb_srcfiles} ${_gr_srcfiles} HEADERS ${_pb_incfiles} ${_gr_incfiles} INCLUDE ${NCBI_INC_ROOT}/${_relpath} PARENT_SCOPE)
        set(${DT_RETURN} DATASPEC ${_specfiles} SOURCES ${_pb_srcfiles} ${_gr_srcfiles} HEADERS ${_pb_incfiles} ${_gr_incfiles} PARENT_SCOPE)
    endif()

    if(NOT DT_GENERATE)
        return()
    endif()

    if ("PROTOBUF" IN_LIST DT_REQUIRES)
        set(_cmd ${NCBI_PROTOC_APP} --cpp_out=${NCBI_SRC_ROOT} --proto_path=${NCBI_SRC_ROOT} ${DT_DATASPEC})
        add_custom_command(
            OUTPUT ${_pb_srcfiles} ${_pb_incfiles}
            COMMAND ${_cmd} VERBATIM
            COMMAND ${CMAKE_COMMAND} -E make_directory ${NCBI_INC_ROOT}/${_relpath}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_path}/${_basename}.pb.h ${NCBI_INC_ROOT}/${_relpath} VERBATIM
            COMMAND ${CMAKE_COMMAND} -E remove -f ${_path}/${_basename}.pb.h VERBATIM
            WORKING_DIRECTORY ${NCBI_TREE_ROOT}
            COMMENT "Generate PROTOC C++ classes from ${DT_DATASPEC}"
            DEPENDS ${DT_DATASPEC}
            VERBATIM
        )
    endif()
    if ("GRPC" IN_LIST DT_REQUIRES)
        set(_cmd ${NCBI_PROTOC_APP} --grpc_out=${NCBI_SRC_ROOT} --proto_path=${NCBI_SRC_ROOT} --plugin=protoc-gen-grpc=${NCBI_GRPC_PLUGIN}  ${DT_DATASPEC})
        add_custom_command(
            OUTPUT ${_gr_srcfiles} ${_gr_incfiles}
            COMMAND ${_cmd} VERBATIM
            COMMAND ${CMAKE_COMMAND} -E make_directory ${NCBI_INC_ROOT}/${_relpath}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_path}/${_basename}.grpc.pb.h ${NCBI_INC_ROOT}/${_relpath} VERBATIM
            COMMAND ${CMAKE_COMMAND} -E remove -f ${_path}/${_basename}.grpc.pb.h VERBATIM
            WORKING_DIRECTORY ${NCBI_TREE_ROOT}
            COMMENT "Generate GRPC C++ classes from ${DT_DATASPEC}"
            DEPENDS ${DT_DATASPEC}
            VERBATIM
        )
    endif()
endfunction()

#############################################################################
NCBI_register_hook(DATASPEC NCBI_internal_process_proto_dataspec ".proto")
