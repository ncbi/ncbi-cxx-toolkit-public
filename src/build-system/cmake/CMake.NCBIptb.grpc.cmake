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
if(NOT NCBI_PATH_CONANGEN)
    find_file(NCBI_PATH_CONANGEN conanrun NAMES conanrun.sh conanrun.bat
        PATHS ${CMAKE_BINARY_DIR}/${NCBI_DIRNAME_CONANGEN} ${CMAKE_BINARY_DIR} ${CMAKE_PREFIX_PATH} ${CMAKE_MODULE_PATH}
        NO_DEFAULT_PATH)
endif()
if(NCBI_PATH_CONANGEN)
    get_filename_component(NCBI_DIR_CONANGEN "${NCBI_PATH_CONANGEN}" DIRECTORY)
    get_filename_component(NCBI_NAME_CONANGEN "${NCBI_PATH_CONANGEN}" NAME)
endif()

##############################################################################
function(NCBI_internal_process_proto_dataspec _variable _access _value)
    if(NOT "${_access}" STREQUAL "MODIFIED_ACCESS" OR "${_value}" STREQUAL "")
        return()
    endif()
    cmake_parse_arguments(DT "GENERATE" "DATASPEC;RETURN" "REQUIRES" ${_value})

    get_filename_component(_path     ${DT_DATASPEC} DIRECTORY)
    get_filename_component(_specname ${DT_DATASPEC} NAME)
    get_filename_component(_basename ${DT_DATASPEC} NAME_WE)
    get_filename_component(_ext      ${DT_DATASPEC} EXT)
    file(RELATIVE_PATH     _relpath  ${NCBI_SRC_ROOT} ${_path})
    set(_codeversion  ${_path}/${_specname}.codever)

    set(_specfiles  ${DT_DATASPEC})
    set(_pb_srcfiles "")
    set(_pb_incfiles "")
    if(NCBI_COMPONENT_PROTOBUF_FOUND)
        if (NCBI_PROTOC_APP)
            set(_pb_srcfiles ${_path}/${_basename}.pb.cc)
            set(_pb_incfiles ${NCBI_INC_ROOT}/${_relpath}/${_basename}.pb.h)
        else()
            message(FATAL_ERROR "Protoc code generator not found: NCBI_PROTOC_APP = ${NCBI_PROTOC_APP}")
        endif()
    endif()
    set(_gr_srcfiles "")
    set(_gr_incfiles "")
    if ("GRPC" IN_LIST DT_REQUIRES)
        if(NCBI_COMPONENT_GRPC_FOUND)
            if (NCBI_GRPC_PLUGIN)
                set(_gr_srcfiles ${_path}/${_basename}.grpc.pb.cc)
                set(_gr_incfiles ${NCBI_INC_ROOT}/${_relpath}/${_basename}.grpc.pb.h ${NCBI_INC_ROOT}/${_relpath}/${_basename}_mock.grpc.pb.h)
            else()
                message(FATAL_ERROR "GRPC C++ plugin code generator not found: NCBI_GRPC_PLUGIN = ${NCBI_GRPC_PLUGIN}")
            endif()
        endif()
    endif()
    if(NOT "${DT_RETURN}" STREQUAL "")
#        set(${DT_RETURN} DATASPEC ${_specfiles} SOURCES ${_pb_srcfiles} ${_gr_srcfiles} HEADERS ${_pb_incfiles} ${_gr_incfiles} INCLUDE ${NCBI_INC_ROOT}/${_relpath} PARENT_SCOPE)
        set(${DT_RETURN} DATASPEC ${_specfiles} SOURCES ${_pb_srcfiles} ${_gr_srcfiles} HEADERS ${_pb_incfiles} ${_gr_incfiles} PARENT_SCOPE)
    endif()

    if(NOT DT_GENERATE)
        return()
    endif()

    if(NCBI_PROTOC_APP_VERSION)
        if(EXISTS "${_codeversion}")
            file(READ "${_codeversion}" _ver)
            if(NOT "${NCBI_PROTOC_APP_VERSION}" STREQUAL "${_ver}")
                file(REMOVE ${_codeversion} ${_pb_srcfiles} ${_gr_srcfiles})
            endif()
        endif()
    endif()

    if(WIN32)
        set(_source "call")
        set(_iferror "if errorlevel 1 exit /b 1")
    else()
        set(_source ".")
        set(_iferror "if test $? -ne 0\; then exit 1\; fi")
    endif()
    set(_app \"${NCBI_PROTOC_APP}\")
    set(_plg \"${NCBI_GRPC_PLUGIN}\")
    set(_cmk \"${CMAKE_COMMAND}\")

    if (NCBI_PROTOC_APP)
        if(WIN32)
            set(_cmdname ${_specname}.pb.bat)
            set(_script)
        else()
            set(_cmdname ${_specname}.pb.sh)
            set(_script "#!/bin/sh\n")
        endif()
        if(NCBI_PATH_CONANGEN)
            string(APPEND _script "${_source} ${NCBI_DIR_CONANGEN}/${NCBI_NAME_CONANGEN}\n")
        endif()
        string(APPEND _script "${_app} --version > \"${_codeversion}\"\n")
        string(APPEND _script "${_app} --cpp_out=. -I. ${_relpath}/${_specname}\n")
        string(APPEND _script "${_iferror}\n")
        if(NCBI_PATH_CONANGEN)
            string(APPEND _script "${_source} ${NCBI_DIR_CONANGEN}/deactivate_${NCBI_NAME_CONANGEN}\n")
        endif()
        if(NOT "${NCBI_SRC_ROOT}" STREQUAL "${NCBI_INC_ROOT}")
            string(APPEND _script "${_cmk} -E make_directory ${NCBI_INC_ROOT}/${_relpath}\n")
            string(APPEND _script "${_cmk} -E copy_if_different ${_path}/${_basename}.pb.h ${NCBI_INC_ROOT}/${_relpath}\n")
            string(APPEND _script "${_cmk} -E remove -f ${_path}/${_basename}.pb.h\n")
        endif()

        string(RANDOM _subdir)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${_subdir})
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${_subdir}/${_cmdname} ${_script})
        file(COPY ${CMAKE_CURRENT_BINARY_DIR}/${_subdir}/${_cmdname} DESTINATION ${CMAKE_CURRENT_BINARY_DIR} FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ)
        file(REMOVE_RECURSE ${CMAKE_CURRENT_BINARY_DIR}/${_subdir})
        add_custom_command(
            OUTPUT ${_pb_srcfiles} ${_pb_incfiles} ${_codeversion}
            COMMAND "${CMAKE_CURRENT_BINARY_DIR}/${_cmdname}"
            WORKING_DIRECTORY ${NCBI_SRC_ROOT}
            COMMENT "Generate PROTOC C++ classes from ${DT_DATASPEC}"
            DEPENDS ${DT_DATASPEC}
            VERBATIM
        )

        if(NOT NCBI_PTBCFG_PACKAGED AND NOT NCBI_PTBCFG_PACKAGING)
            file(APPEND ${NCBI_GENERATESRC_GRPC} "echo ${NCBI_SRC_ROOT}/${_relpath}/${_basename}${_ext}\n")
            file(APPEND ${NCBI_GENERATESRC_GRPC} "cd ${NCBI_SRC_ROOT}\n")
            file(APPEND ${NCBI_GENERATESRC_GRPC} "${_app} --version > \"${_codeversion}\"\n")
            file(APPEND ${NCBI_GENERATESRC_GRPC} "${_app} --cpp_out=. -I. ${_relpath}/${_basename}${_ext}\n")
            if(WIN32)
                file(APPEND ${NCBI_GENERATESRC_GRPC} "if errorlevel 1 (set GENERATESRC_RESULT=1)\n")
            else()
                file(APPEND ${NCBI_GENERATESRC_GRPC} "test $? -eq 0 || GENERATESRC_RESULT=1\n")
            endif()
            if(NOT "${NCBI_SRC_ROOT}" STREQUAL "${NCBI_INC_ROOT}")
                file(APPEND ${NCBI_GENERATESRC_GRPC} "${_cmk} -E make_directory ${NCBI_INC_ROOT}/${_relpath}\n")
                file(APPEND ${NCBI_GENERATESRC_GRPC} "${_cmk} -E copy_if_different ${_path}/${_basename}.pb.h ${NCBI_INC_ROOT}/${_relpath}\n")
                file(APPEND ${NCBI_GENERATESRC_GRPC} "${_cmk} -E remove -f ${_path}/${_basename}.pb.h\n")
            endif()
        endif()
    endif()
    if ("GRPC" IN_LIST DT_REQUIRES AND NCBI_PROTOC_APP AND NCBI_GRPC_PLUGIN)
        if(WIN32)
            set(_cmdname ${_specname}.grpc.bat)
            set(_script)
        else()
            set(_cmdname ${_specname}.grpc.sh)
            set(_script "#!/bin/sh\n")
        endif()
        if(NCBI_PATH_CONANGEN)
            string(APPEND _script "${_source} ${NCBI_DIR_CONANGEN}/${NCBI_NAME_CONANGEN}\n")
        endif()
        string(APPEND _script "${_app} --grpc_out=generate_mock_code=true:. --plugin=protoc-gen-grpc=${_plg} -I. ${_relpath}/${_specname}\n")
        string(APPEND _script "${_iferror}\n")
        if(NCBI_PATH_CONANGEN)
            string(APPEND _script "${_source} ${NCBI_DIR_CONANGEN}/deactivate_${NCBI_NAME_CONANGEN}\n")
        endif()
        if(NOT "${NCBI_SRC_ROOT}" STREQUAL "${NCBI_INC_ROOT}")
            string(APPEND _script "${_cmk} -E make_directory ${NCBI_INC_ROOT}/${_relpath}\n")
            string(APPEND _script "${_cmk} -E copy_if_different ${_path}/${_basename}.grpc.pb.h ${NCBI_INC_ROOT}/${_relpath}\n")
            string(APPEND _script "${_cmk} -E copy_if_different ${_path}/${_basename}_mock.grpc.pb.h ${NCBI_INC_ROOT}/${_relpath}\n")
            string(APPEND _script "${_cmk} -E remove -f ${_path}/${_basename}.grpc.pb.h\n")
            string(APPEND _script "${_cmk} -E remove -f ${_path}/${_basename}_mock.grpc.pb.h\n")
        endif()

        string(RANDOM _subdir)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${_subdir})
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${_subdir}/${_cmdname} ${_script})
        file(COPY ${CMAKE_CURRENT_BINARY_DIR}/${_subdir}/${_cmdname} DESTINATION ${CMAKE_CURRENT_BINARY_DIR} FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ)
        file(REMOVE_RECURSE ${CMAKE_CURRENT_BINARY_DIR}/${_subdir})
        add_custom_command(
            OUTPUT ${_gr_srcfiles} ${_gr_incfiles}
            COMMAND "${CMAKE_CURRENT_BINARY_DIR}/${_cmdname}"
            WORKING_DIRECTORY ${NCBI_SRC_ROOT}
            COMMENT "Generate GRPC C++ classes from ${DT_DATASPEC}"
            DEPENDS ${DT_DATASPEC}
            VERBATIM
        )

        if(NOT NCBI_PTBCFG_PACKAGED AND NOT NCBI_PTBCFG_PACKAGING)
            file(APPEND ${NCBI_GENERATESRC_GRPC} "echo ${NCBI_SRC_ROOT}/${_relpath}/${_basename}${_ext}\n")
            file(APPEND ${NCBI_GENERATESRC_GRPC} "cd ${NCBI_SRC_ROOT}\n")
            file(APPEND ${NCBI_GENERATESRC_GRPC} "${_app} --grpc_out=generate_mock_code=true:. --plugin=protoc-gen-grpc=${_plg} -I. ${_relpath}/${_basename}${_ext}\n")
            if(WIN32)
                file(APPEND ${NCBI_GENERATESRC_GRPC} "if errorlevel 1 (set GENERATESRC_RESULT=1)\n")
            else()
                file(APPEND ${NCBI_GENERATESRC_GRPC} "test $? -eq 0 || GENERATESRC_RESULT=1\n")
            endif()
            if(NOT "${NCBI_SRC_ROOT}" STREQUAL "${NCBI_INC_ROOT}")
                file(APPEND ${NCBI_GENERATESRC_GRPC} "${_cmk} -E make_directory ${NCBI_INC_ROOT}/${_relpath}\n")
                file(APPEND ${NCBI_GENERATESRC_GRPC} "${_cmk} -E copy_if_different ${_path}/${_basename}.grpc.pb.h ${NCBI_INC_ROOT}/${_relpath}\n")
                file(APPEND ${NCBI_GENERATESRC_GRPC} "${_cmk} -E copy_if_different ${_path}/${_basename}_mock.grpc.pb.h ${NCBI_INC_ROOT}/${_relpath}\n")
                file(APPEND ${NCBI_GENERATESRC_GRPC} "${_cmk} -E remove -f ${_path}/${_basename}.grpc.pb.h\n")
                file(APPEND ${NCBI_GENERATESRC_GRPC} "${_cmk} -E remove -f ${_path}/${_basename}_mock.grpc.pb.h\n")
            endif()
        endif()
    endif()
    if (NOT "${_pb_srcfiles}" STREQUAL "")
        NCBI_util_gitignore(${_pb_srcfiles} ${_gr_srcfiles} ${_codeversion})
    endif()
    if (NOT "${_pb_incfiles}" STREQUAL "")
        NCBI_util_gitignore(${_pb_incfiles} ${_gr_incfiles})
    endif()
endfunction()

##############################################################################
function(NCBI_internal_adjust_proto_target _variable _access _value)
    if(NOT "${_access}" STREQUAL "MODIFIED_ACCESS" OR "${_value}" STREQUAL "")
        return()
    endif()
    if( (DEFINED NCBI_${NCBI_PROJECT}_REQUIRES AND (GRPC IN_LIST NCBI_${NCBI_PROJECT}_REQUIRES OR PROTOBUF IN_LIST NCBI_${NCBI_PROJECT}_REQUIRES)) OR
        (DEFINED NCBI_${NCBI_PROJECT}_COMPONENTS AND (GRPC IN_LIST NCBI_${NCBI_PROJECT}_COMPONENTS OR PROTOBUF IN_LIST NCBI_${NCBI_PROJECT}_COMPONENTS)))
        if(DEFINED NCBI_${NCBI_PROJECT}_DATASPEC)
            foreach(_spec IN LISTS NCBI_${NCBI_PROJECT}_DATASPEC)
                if("${_spec}" MATCHES "[.]proto$")
                    set_property(TARGET ${NCBI_PROJECT} PROPERTY CXX_STANDARD 14)
                    return()
                endif()
            endforeach()
        endif()
    endif()
endfunction()

#############################################################################
NCBI_register_hook(DATASPEC NCBI_internal_process_proto_dataspec ".proto")
#NCBI_register_hook(TARGET_ADDED NCBI_internal_adjust_proto_target)

if(NOT NCBI_PTBCFG_PACKAGED AND NOT NCBI_PTBCFG_PACKAGING)
    set(NCBI_GENERATESRC_GRPC   ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}/CMakeFiles/generate_sources.grpc)
    if (EXISTS ${NCBI_GENERATESRC_GRPC})
        file(REMOVE ${NCBI_GENERATESRC_GRPC})
    endif()
endif()
