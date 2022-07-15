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
    if("${_value}" MATCHES "Debug")
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
        foreach( _item IN LISTS _hostinfo)
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
macro(NCBI_util_ToCygwinPath _value _result)
    set(${_result} "${_value}")
    if(WIN32)
        string(FIND ${_value} ":" _pos)
        if(${_pos} EQUAL 1)
            string(REPLACE ":" ""  _tmp "${_value}")
            set(${_result} "/cygdrive/${_tmp}")
        endif()
    endif()
endmacro()

##############################################################################
macro(NCBI_Subversion_WC_INFO _dir _prefix)
    if(CYGWIN)
        NCBI_util_ToCygwinPath(${_dir} _tmp)
        Subversion_WC_INFO(${_tmp} ${_prefix})
    else()
        Subversion_WC_INFO(${_dir} ${_prefix})
    endif()
endmacro()
