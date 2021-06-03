#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper extension
##  In NCBI CMake wrapper, adds support of datatool code generation
##    Author: Andrei Gourianov, gouriano@ncbi
##


##############################################################################
# Find datatool app

if (WIN32)
    set(NCBI_DATATOOL_BASE "//snowman/win-coremake/App/Ncbi/cppcore/datatool/msvc")
elseif(APPLE)
    set(NCBI_DATATOOL_BASE "/net/snowman/vol/export2/win-coremake/App/Ncbi/cppcore/datatool/XCode;/Volumes/win-coremake/App/Ncbi/cppcore/datatool/XCode")
else()
    set(NCBI_DATATOOL_BASE "/net/snowman/vol/export2/win-coremake/App/Ncbi/cppcore/datatool")
    if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        set(NCBI_DATATOOL_BASE "${NCBI_DATATOOL_BASE}/Linux64")
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
        set(NCBI_DATATOOL_BASE "${NCBI_DATATOOL_BASE}/FreeBSD64")
    endif()
endif()

if (EXISTS "${NCBI_TREE_BUILDCFG}/datatool_version.txt")
    FILE(STRINGS "${NCBI_TREE_BUILDCFG}/datatool_version.txt" _datatool_version)
else()
    set(_datatool_version "2.20.0")
    message(WARNING "Failed to find datatool_version.txt, defaulting to version ${_datatool_version})")
endif()
message(STATUS "Datatool version required by software: ${_datatool_version}")

if (WIN32)
    set(NCBI_DATATOOL_BIN "datatool.exe")
else()
    set(NCBI_DATATOOL_BIN "datatool")
endif()

if(NOT DEFINED NCBI_DATATOOL)
    foreach(_base IN LISTS NCBI_DATATOOL_BASE)
        if (EXISTS "${_base}/${_datatool_version}/${NCBI_DATATOOL_BIN}")
            set (NCBI_DATATOOL "${_base}/${_datatool_version}/${NCBI_DATATOOL_BIN}")
            message(STATUS "Datatool location: ${NCBI_DATATOOL}")
            break()
        endif()
    endforeach()
endif()
if (NOT EXISTS "${NCBI_DATATOOL}")
    set (NCBI_DATATOOL datatool)
    message(STATUS "Datatool location: <locally compiled>")
endif()

##############################################################################
function(NCBI_internal_process_dataspec _variable _access _value)
    if(NOT "${_access}" STREQUAL "MODIFIED_ACCESS" OR "${_value}" STREQUAL "")
        return()
    endif()
    cmake_parse_arguments(DT "GENERATE" "DATASPEC;RETURN" "REQUIRES" ${_value})

    get_filename_component(_path     ${DT_DATASPEC} DIRECTORY)
    get_filename_component(_basename ${DT_DATASPEC} NAME_WE)
    get_filename_component(_ext      ${DT_DATASPEC} EXT)
    file(RELATIVE_PATH     _relpath  ${NCBI_SRC_ROOT} ${_path})

    set(_specfiles  ${DT_DATASPEC})
    set(_srcfiles   ${_path}/${_basename}__.cpp ${_path}/${_basename}___.cpp)
    set(_incfiles   ${NCBI_INC_ROOT}/${_relpath}/${_basename}__.hpp)
    if(NOT "${DT_RETURN}" STREQUAL "")
        set(${DT_RETURN} DATASPEC ${_specfiles} SOURCES ${_srcfiles} HEADERS ${_incfiles} PARENT_SCOPE)
    endif()

    if(NOT DT_GENERATE)
        return()
    endif()

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
            set(_imports -M ${_module_imports})
        endif()
    endif()

    NCBI_util_get_value(PCH _fpch)
    if (NOT "${_fpch}" STREQUAL "")
        set(_pch -pch ${_fpch})
    endif()

    set(_oc ${_basename})
    set(_od ${_path}/${_basename}.def)
    set(_oex -oex " ")
    set(_cmd ${NCBI_DATATOOL} ${_oex} ${_pch} -m ${DT_DATASPEC} -oA -oc ${_oc} -od ${_od} -odi -ocvs -or ${_relpath} -oR ${NCBI_TREE_ROOT} ${_imports})
    set(_depends ${NCBI_DATATOOL} ${DT_DATASPEC})
    if(EXISTS ${_od})
        set(_depends ${_depends} ${_od})
    endif()
    add_custom_command(
        OUTPUT ${_srcfiles} ${_incfiles} ${_path}/${_basename}.files
        COMMAND ${_cmd} VERBATIM
        WORKING_DIRECTORY ${NCBI_TREE_ROOT}
        COMMENT "Generate C++ classes from ${DT_DATASPEC}"
        DEPENDS ${_depends}
        VERBATIM
    )

    if (NOT "${_module_imports}" STREQUAL "")
        set(_imports "-M \"${_module_imports}\"")
    else()
        set(_imports "")
    endif()
    if (NOT "${_fpch}" STREQUAL "")
        set(_pch "-pch ${_fpch}")
    else()
        set(_pch "")
    endif()
    if(WIN32)
        set(_app \"${NCBI_DATATOOL}\")
    else()
        set(_app ${NCBI_DATATOOL})
    endif()
    file(APPEND ${NCBI_GENERATESRC_DATATOOL} "${_app} -oex \"\" ${_pch} -m \"${DT_DATASPEC}\" -oA -oc ${_oc} -od \"${_od}\" -odi -ocvs -or ${_relpath} -oR \"${NCBI_TREE_ROOT}\" ${_imports}\n")
endfunction()

#############################################################################
function(NCBI_internal_allow_datatool)
    get_property(_allowedprojects GLOBAL PROPERTY NCBI_PTBPROP_ALLOWED_PROJECTS)
    if(DEFINED NCBI_EXTERNAL_TREE_ROOT)
        if("${NCBI_DATATOOL}.local" IN_LIST _allowedprojects)
            set(NCBI_DATATOOL "${NCBI_DATATOOL}.local" PARENT_SCOPE)
        endif()
    else()
        set(_allowedprojects ${_allowedprojects} ${NCBI_DATATOOL})
        set_property(GLOBAL PROPERTY NCBI_PTBPROP_ALLOWED_PROJECTS ${_allowedprojects})
    endif()
endfunction()

#############################################################################
function(NCBI_internal_finalize_generatesrc)
    get_filename_component(_srcpath  ${NCBI_GENERATESRC_DATATOOL} DIRECTORY)
    file(GLOB _files LIST_DIRECTORIES FALSE "${_srcpath}/generate_sources.*")
    if(WIN32)
        set(_dest "${_srcpath}/generate_sources.bat")
        file(WRITE ${_dest} "\n")
    else()
        set(_dest "${_srcpath}/generate_sources")
        file(WRITE ${_dest} "#!/bin/sh\n")
    endif()
    foreach(_file IN LISTS _files)
        file(READ ${_file} _cmd)
        file(APPEND ${_dest} ${_cmd})
        file(REMOVE ${_file})
    endforeach()
    file(COPY ${_dest} DESTINATION "${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}"
         FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    file(REMOVE ${_dest})
endfunction()

#############################################################################
if(NOT IS_ABSOLUTE "${NCBI_DATATOOL}")
    NCBI_register_hook(ALL_PARSED NCBI_internal_allow_datatool)
endif()
NCBI_register_hook(DATASPEC NCBI_internal_process_dataspec ".asn;.dtd;.xsd;.wsdl;.jsd;.json")

set(NCBI_GENERATESRC_DATATOOL   ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}/CMakeFiles/generate_sources.datatool)
if (EXISTS ${NCBI_GENERATESRC_DATATOOL})
    file(REMOVE ${NCBI_GENERATESRC_DATATOOL})
endif()
file(WRITE ${NCBI_GENERATESRC_DATATOOL} "cd ${NCBI_TREE_ROOT}\n")
NCBI_register_hook(ALL_ADDED NCBI_internal_finalize_generatesrc)
