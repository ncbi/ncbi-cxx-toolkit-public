#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper
##    Author: Andrei Gourianov, gouriano@ncbi
##


##############################################################################
macro(NCBI_util_parse_sign _input _value _negative)
    string(SUBSTRING ${_input} 0 1 _sign)
    if ("${_sign}" STREQUAL "-")
        string(SUBSTRING ${_input} 1 -1 ${_value})
        set(${_negative} ON)
    else()
        set(${_value} ${_input})
        set(${_negative} OFF)
    endif()
endmacro()

##############################################################################
macro(NCBI_util_Cfg_ToStd _value _result)
    if("${_value}" MATCHES "[Dd]ebug")
        set(${_result} Debug)
    else()
        set(${_result} Release)
    endif()
endmacro()

##############################################################################
function(NCBI_process_imports _file)
    if(NOT EXISTS ${_file})
        return()
    endif()
    if(NOT "${NCBI_CONFIGURATION_TYPES_COUNT}" EQUAL 1)
        return()
    endif()
    NCBI_util_Cfg_ToStd(${NCBI_CONFIGURATION_TYPES} _std_cfg)
    set(_std_cfg ${_std_cfg}${NCBI_CONFIGURATION_RUNTIMELIB})
    string(TOUPPER ${NCBI_CONFIGURATION_TYPES} _up_cfg)
    string(TOUPPER ${_std_cfg} _up_std_cfg)

    file(STRINGS ${_file} _hostinfo)
    if (NOT "${_hostinfo}" STREQUAL "")
        foreach( _prj IN LISTS _hostinfo)
            string(REPLACE " " ";" _prj ${_prj})
            list(GET _prj 0 _item)
            if (TARGET "${_item}")
                if(WIN32)
                    get_target_property(_deps ${_item} INTERFACE_LINK_LIBRARIES)
                    if(_deps)
                        string(FIND "${_deps}" "\$<CONFIG>" _pos)
                        if(${_pos} GREATER "-1")
                            string(REPLACE ";" "?" _deps "${_deps}")
                            string(REPLACE "\$<CONFIG>" "${_std_cfg}" _deps "${_deps}")
                            string(REPLACE "?" ";" _deps "${_deps}")
                            set_target_properties(${_item} PROPERTIES INTERFACE_LINK_LIBRARIES "${_deps}")
                        endif()
                    endif()
                    get_target_property(_deps ${_item} IMPORTED_IMPLIB_${_up_std_cfg})
                    if(_deps)
                        set_target_properties(${_item} PROPERTIES IMPORTED_IMPLIB_${_up_cfg} "${_deps}")
                    endif()
                endif()
                get_target_property(_deps ${_item} IMPORTED_LOCATION_${_up_std_cfg})
                if(_deps)
                    set_target_properties(${_item} PROPERTIES IMPORTED_LOCATION_${_up_cfg} "${_deps}")
                endif()
            endif()
        endforeach()
    endif()
endfunction()

##############################################################################
# assume all files are in the same directory
function(NCBI_util_gitignore _f)
    set( _files ${ARGV})
    if("${_files}" STREQUAL "")
        return()
    endif()
    list(GET _files 0 _file)
    get_filename_component(_dir ${_file} DIRECTORY)
    set(_git ${_dir}/.gitignore)
    if(EXISTS ${_git})
        file(STRINGS ${_git} _allfiles)
    else()
        set(_allfiles .gitignore)
    endif()
    list(APPEND _allfiles ${_files})
    set(_result)
    foreach(_file IN LISTS _allfiles)
        get_filename_component(_name ${_file} NAME)
        list(APPEND _result "${_name}\n")
    endforeach()
    list(REMOVE_DUPLICATES  _result)
    file(WRITE ${_dir}/.gitignore ${_result})
endfunction()
