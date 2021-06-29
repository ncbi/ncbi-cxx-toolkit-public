#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper extension
##  In NCBI CMake wrapper, adds installation commands
##    Author: Andrei Gourianov, gouriano@ncbi
##


##############################################################################
function(NCBI_internal_install_target _variable _access)
    if(NOT "${_access}" STREQUAL "MODIFIED_ACCESS")
        return()
    endif()
    if(NCBI_PTBCFG_PACKAGE)
        set(NCBI_DIRNAME_PREBUILT ".")
    endif()

    if (${NCBI_${NCBI_PROJECT}_TYPE} STREQUAL "STATIC")
        set(_doexport YES)
        set(_haspdb NO)
        set(_dest ${NCBI_DIRNAME_PREBUILT}/${NCBI_DIRNAME_ARCHIVE})
        if(WIN32)
            set_target_properties(${NCBI_PROJECT} PROPERTIES COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
        endif()
    elseif (${NCBI_${NCBI_PROJECT}_TYPE} STREQUAL "SHARED")
        set(_doexport YES)
        set(_haspdb YES)
        if (WIN32)
            set(_dest    ${NCBI_DIRNAME_PREBUILT}/${NCBI_DIRNAME_SHARED})
            set(_dest_ar ${NCBI_DIRNAME_PREBUILT}/${NCBI_DIRNAME_ARCHIVE})
        else()
            set(_dest ${NCBI_DIRNAME_PREBUILT}/${NCBI_DIRNAME_ARCHIVE})
        endif()
    elseif (${NCBI_${NCBI_PROJECT}_TYPE} STREQUAL "CONSOLEAPP" OR ${NCBI_${NCBI_PROJECT}_TYPE} STREQUAL "GUIAPP")
        set(_doexport NO)
        set(_haspdb YES)
        set(_dest ${NCBI_DIRNAME_PREBUILT}/${NCBI_DIRNAME_RUNTIME})
        if (NOT "${NCBI_PTBCFG_INSTALL_TAGS}" STREQUAL "")
            set(_alltags ${NCBI__PROJTAG} ${NCBI_${NCBI_PROJECT}_PROJTAG})
            set(_res FALSE)
            set(_hasp FALSE)
            foreach(_tag IN LISTS NCBI_PTBCFG_INSTALL_TAGS)
                NCBI_util_parse_sign( ${_tag} _value _negate)
                if(_negate)
                    if( ${_value} IN_LIST _alltags)
                        if(NCBI_VERBOSE_ALLPROJECTS OR NCBI_VERBOSE_PROJECT_${NCBI_PROJECT})
                            message("${NCBI_PROJECT} will not be installed because of tag ${_value}")
                        endif()
                        return()
                    endif()
                else()
                    set(_hasp TRUE)
                    if( ${_value} IN_LIST _alltags OR ${_value} STREQUAL "*")
                        set(_res TRUE)
                    endif()
                endif()
            endforeach()
            if(_hasp AND NOT _res)
                if(NCBI_VERBOSE_ALLPROJECTS OR NCBI_VERBOSE_PROJECT_${NCBI_PROJECT})
                    message("${NCBI_PROJECT} will not be installed because of tags ${_alltags}")
                endif()
                return()
            endif()
        endif()
        if(${NCBI_PROJECT} MATCHES "test|demo|sample")
            if(NCBI_VERBOSE_ALLPROJECTS OR NCBI_VERBOSE_PROJECT_${NCBI_PROJECT})
                message("${NCBI_PROJECT} will not be installed because of its name")
            endif()
            return()
        endif()
    else()
        return()
    endif()
    if ("${_dest}" STREQUAL "")
        return()
    endif()
    set_property(GLOBAL APPEND PROPERTY NCBI_PTBPROP_EXPORTED_TARGETS ${NCBI_PROJECT})

# not sure about this part
    file(RELATIVE_PATH _rel "${NCBI_SRC_ROOT}" "${NCBI_CURRENT_SOURCE_DIR}")
    string(REPLACE "/" ";" _rel ${_rel})
    list(GET _rel 0 _dir)
    get_property(_all_subdirs GLOBAL PROPERTY NCBI_PTBPROP_ROOT_SUBDIR)
    list(APPEND _all_subdirs ${_dir})
    if (DEFINED NCBI_${NCBI_PROJECT}_PARTS)
        foreach(_rel IN LISTS NCBI_${NCBI_PROJECT}_PARTS)
            string(REPLACE "/" ";" _rel ${_rel})
            list(GET _rel 0 _dir)
            list(APPEND _all_subdirs ${_dir})
        endforeach()
    endif()
    list(REMOVE_DUPLICATES _all_subdirs)
    set_property(GLOBAL PROPERTY NCBI_PTBPROP_ROOT_SUBDIR ${_all_subdirs})

    file(RELATIVE_PATH _rel "${NCBI_SRC_ROOT}" "${NCBI_CURRENT_SOURCE_DIR}")
    get_property(_all_subdirs GLOBAL PROPERTY NCBI_PTBPROP_SOURCE_DIR)
    list(APPEND _all_subdirs ${_rel})
    if (DEFINED NCBI_${NCBI_PROJECT}_PARTS)
        foreach(_rel IN LISTS NCBI_${NCBI_PROJECT}_PARTS)
            get_filename_component(_rel ${_rel} DIRECTORY)
            list(APPEND _all_subdirs ${_rel})
        endforeach()
    endif()
    list(SORT _all_subdirs)
    list(REMOVE_DUPLICATES _all_subdirs)
    set_property(GLOBAL PROPERTY NCBI_PTBPROP_SOURCE_DIR ${_all_subdirs})

    if(NOT NCBI_PTBCFG_PACKAGE)
        NCBI_util_get_value(PROPERTY_EXPORT _entry)
        if (NOT "${_entry}" STREQUAL "")
            set(_doexport ${_entry})
        endif()
    endif()
    set(_exp "")
    if (WIN32 OR XCODE)
        foreach(_cfg IN LISTS NCBI_CONFIGURATION_TYPES)
            if(_doexport)
                set(_exp EXPORT ${NCBI_PTBCFG_INSTALL_EXPORT}${_cfg})
            endif()
            if (DEFINED _dest_ar)
                install(
                    TARGETS ${NCBI_PROJECT}
                    ${_exp}
                    RUNTIME DESTINATION ${_dest}/${_cfg}
                    CONFIGURATIONS ${_cfg}
                    ARCHIVE DESTINATION ${_dest_ar}/${_cfg}
                    CONFIGURATIONS ${_cfg}
                )
            else()
                install(
                    TARGETS ${NCBI_PROJECT}
                    ${_exp}
                    DESTINATION ${_dest}/${_cfg}
                    CONFIGURATIONS ${_cfg}
                )
            endif()
            if (WIN32 AND NOT NCBI_PTBCFG_PACKAGE)
                if (_haspdb)
                    install(FILES $<TARGET_PDB_FILE:${NCBI_PROJECT}>
                            DESTINATION ${_dest}/${_cfg} OPTIONAL
                            CONFIGURATIONS ${_cfg})
                else()
                    install(FILES $<TARGET_FILE:${NCBI_PROJECT}>.pdb
                            DESTINATION ${_dest}/${_cfg} OPTIONAL
                            CONFIGURATIONS ${_cfg})
                endif()
            endif()
        endforeach()
    else()
        if(_doexport)
            set(_exp EXPORT ${NCBI_PTBCFG_INSTALL_EXPORT})
        endif()
        install(
            TARGETS ${NCBI_PROJECT}
            ${_exp}
            DESTINATION ${_dest}
        )
    endif()
endfunction()

##############################################################################
function(NCBI_internal_export_imports _file)
    if(EXISTS ${_file})
        file(REMOVE ${_file})
    endif()
    get_property(_allprojects     GLOBAL PROPERTY NCBI_PTBPROP_EXPORTED_TARGETS)
    if (NOT "${_allprojects}" STREQUAL "")
        set(_hostinfo)
        foreach(_prj IN LISTS _allprojects)
            list(APPEND _hostinfo "${_prj}\n")
        endforeach()
        if (NOT "${_hostinfo}" STREQUAL "")
            file(WRITE ${_file} ${_hostinfo})
        endif()
    endif()
endfunction()

##############################################################################
function(NCBI_internal_export_hostinfo _file)
    if(EXISTS ${_file})
        file(REMOVE ${_file})
    endif()
    get_property(_allprojects     GLOBAL PROPERTY NCBI_PTBPROP_ALL_PROJECTS)
    if (NOT "${_allprojects}" STREQUAL "")
        set(_hostinfo)
        foreach(_prj IN LISTS _allprojects)
            get_property(_prjhost GLOBAL PROPERTY NCBI_PTBPROP_HOST_${_prj})
            if (NOT "${_prjhost}" STREQUAL "")
                list(APPEND _hostinfo "${_prj} ${_prjhost}\n")
            endif()
        endforeach()
        if (NOT "${_hostinfo}" STREQUAL "")
            file(WRITE ${_file} ${_hostinfo})
        endif()
    endif()
endfunction()

##############################################################################
function(NCBI_internal_export_buildinfo _file)
    if(EXISTS ${_file})
        file(REMOVE ${_file})
    endif()
    set(_buildinfo)
    if (MSVC)
        list(APPEND _buildinfo "set CMAKE_GENERATOR=${CMAKE_GENERATOR}\n")
        list(APPEND _buildinfo "set CMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}\n")
        list(APPEND _buildinfo "set BUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}\n")
    elseif(XCODE)
        list(APPEND _buildinfo "CMAKE_GENERATOR=\"${CMAKE_GENERATOR}\"\n")
        list(APPEND _buildinfo "BUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}\n")
    else()
        list(APPEND _buildinfo "CMAKE_C_COMPILER=${CMAKE_C_COMPILER}\n")
        list(APPEND _buildinfo "CMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}\n")
        list(APPEND _buildinfo "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}\n")
        list(APPEND _buildinfo "BUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}\n")
    endif()
    file(WRITE ${_file} ${_buildinfo})
endfunction()

##############################################################################
function(NCBI_internal_install_root _variable _access)
    if(NOT "${_access}" STREQUAL "MODIFIED_ACCESS")
        return()
    endif()
    if(NCBI_PTBCFG_PACKAGE)
        set(_dest ${NCBI_DIRNAME_EXPORT})
    else()
        set(_dest ${NCBI_DIRNAME_PREBUILT}/${NCBI_DIRNAME_EXPORT})
    endif()

    set(_imports ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}/${CMAKE_PROJECT_NAME}.imports)
    NCBI_internal_export_imports(${_imports})
    if (EXISTS ${_imports})
        install( FILES ${_imports} DESTINATION ${_dest} RENAME ${NCBI_PTBCFG_INSTALL_EXPORT}.imports)
    endif()
    set(_hostinfo ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}/${CMAKE_PROJECT_NAME}.hostinfo)
    NCBI_internal_export_hostinfo(${_hostinfo})
    if (EXISTS ${_hostinfo})
        install( FILES ${_hostinfo} DESTINATION ${_dest} RENAME ${NCBI_PTBCFG_INSTALL_EXPORT}.hostinfo)
    endif()
    set(_buildinfo ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}/${CMAKE_PROJECT_NAME}.buildinfo)
    NCBI_internal_export_buildinfo(${_buildinfo})
    if (EXISTS ${_buildinfo})
        install( FILES ${_buildinfo} DESTINATION ${_dest} RENAME buildinfo)
    endif()
    if (NCBI_PTBCFG_PACKAGE AND EXISTS ${NCBI_TREE_ROOT}/doc/public/LICENSE)
        install( FILES ${NCBI_TREE_ROOT}/doc/public/LICENSE DESTINATION licenses)
    endif()

    if (WIN32 OR XCODE)
        foreach(_cfg IN LISTS NCBI_CONFIGURATION_TYPES)
            install(EXPORT ${NCBI_PTBCFG_INSTALL_EXPORT}${_cfg}
                CONFIGURATIONS ${_cfg}
                DESTINATION ${_dest}
                FILE ${NCBI_PTBCFG_INSTALL_EXPORT}.cmake
            )
        endforeach()
    else()
        install(EXPORT ${NCBI_PTBCFG_INSTALL_EXPORT}
            DESTINATION ${_dest}
            FILE ${NCBI_PTBCFG_INSTALL_EXPORT}.cmake
        )
    endif()

# install headers
    get_property(_all_subdirs GLOBAL PROPERTY NCBI_PTBPROP_ROOT_SUBDIR)
    list(APPEND _all_subdirs ${NCBI_DIRNAME_COMMON_INCLUDE})
    foreach(_dir IN LISTS _all_subdirs)
        if (EXISTS ${NCBI_INC_ROOT}/${_dir})
            if(NCBI_PTBCFG_PACKAGE)
                install( DIRECTORY ${NCBI_INC_ROOT}/${_dir} DESTINATION ${NCBI_DIRNAME_INCLUDE}
                    REGEX "/[.]svn$" EXCLUDE)
            else()
                install( DIRECTORY ${NCBI_INC_ROOT}/${_dir} DESTINATION ${NCBI_DIRNAME_INCLUDE}
                    REGEX "/[.].*$" EXCLUDE)
            endif()
        endif()
    endforeach()
    file(GLOB _files LIST_DIRECTORIES false "${NCBI_INC_ROOT}/*")
    install( FILES ${_files} DESTINATION ${NCBI_DIRNAME_INCLUDE})

    if(NCBI_PTBCFG_PACKAGE)
        if (WIN32 OR XCODE)
            if(DEFINED CONAN_SETTINGS_BUILD_TYPE)
                install( DIRECTORY ${NCBI_CFGINC_ROOT}/${CONAN_SETTINGS_BUILD_TYPE}/ DESTINATION ${NCBI_DIRNAME_INCLUDE})
            else()
                install( DIRECTORY ${NCBI_CFGINC_ROOT}/Release/ DESTINATION ${NCBI_DIRNAME_INCLUDE})
            endif()
        else()
            install( DIRECTORY ${NCBI_CFGINC_ROOT}/ DESTINATION ${NCBI_DIRNAME_INCLUDE})
        endif()
        return()
    endif()
    set(_dest ${NCBI_DIRNAME_PREBUILT})

    install( DIRECTORY ${NCBI_CFGINC_ROOT} DESTINATION "${_dest}"
            REGEX "/[.].*$" EXCLUDE)

# install sources
    if ($ENV{NCBIPTB_INSTALL_SRC})
        get_property(_all_subdirs GLOBAL PROPERTY PROPERTY NCBI_PTBPROP_SOURCE_DIR)
        foreach(_dir IN LISTS _all_subdirs)
            if (EXISTS ${NCBI_SRC_ROOT}/${_dir})
                get_filename_component(_path ${_dir} DIRECTORY)
                if(NOT "${_path}" STREQUAL "")
                    set(_path "/${_path}")
                endif()
                install( DIRECTORY ${NCBI_SRC_ROOT}/${_dir} DESTINATION ${NCBI_DIRNAME_SRC}${_path}
                    FILES_MATCHING  PATTERN "*.c" PATTERN "*.c?" PATTERN "*.c??" 
                    REGEX "/[.].*$" EXCLUDE REGEX "${NCBI_SRC_ROOT}/${_dir}/.*/[^/]" EXCLUDE)
            endif()
        endforeach()
        file(GLOB _files LIST_DIRECTORIES false "${NCBI_SRC_ROOT}/*")
        install( FILES ${_files} DESTINATION ${NCBI_DIRNAME_SRC})
    endif()

    file(GLOB _files LIST_DIRECTORIES false "${NCBI_TREE_BUILDCFG}/*")
    install( FILES ${_files} DESTINATION ${NCBI_DIRNAME_BUILDCFG})
    install( DIRECTORY ${NCBI_TREE_CMAKECFG} DESTINATION ${NCBI_DIRNAME_BUILDCFG}
            USE_SOURCE_PERMISSIONS REGEX "/[.].*$" EXCLUDE)

    install( DIRECTORY ${NCBI_TREE_ROOT}/${NCBI_DIRNAME_SCRIPTS} DESTINATION "."
            USE_SOURCE_PERMISSIONS REGEX "/[.].*$" EXCLUDE)

# test results
    if ($ENV{NCBIPTB_INSTALL_CHECK})
        install( DIRECTORY ${NCBI_BUILD_ROOT}/check DESTINATION "${_dest}")
        install( DIRECTORY ${NCBI_BUILD_ROOT}/testing DESTINATION "${_dest}")
    else()
        if (WIN32 OR XCODE)
            install( DIRECTORY ${NCBI_BUILD_ROOT}/check DESTINATION "${_dest}"
                REGEX "${NCBI_BUILD_ROOT}/check/.*/.*/[^/]" EXCLUDE
            )
#            install( DIRECTORY ${NCBI_BUILD_ROOT}/testing DESTINATION "${_dest}"
#                REGEX "${NCBI_BUILD_ROOT}/testing/.*/.*/[^/]" EXCLUDE
#            )
        else()
            install( DIRECTORY ${NCBI_BUILD_ROOT}/check DESTINATION "${_dest}"
                REGEX "${NCBI_BUILD_ROOT}/check/.*/[^/]" EXCLUDE
            )
#            install( DIRECTORY ${NCBI_BUILD_ROOT}/testing DESTINATION "${_dest}"
#                REGEX "${NCBI_BUILD_ROOT}/testing/.*/[^/]" EXCLUDE
#            )
        endif()
    endif()
    if (WIN32 OR XCODE)
        install( DIRECTORY ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}/Testing/Temporary/ DESTINATION "${_dest}/testing/$<CONFIG>")
    else()
        install( DIRECTORY ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}/Testing/Temporary/ DESTINATION "${_dest}/testing")
    endif()

#install build info files
    set(_files "${NCBI_TREE_ROOT}/build_info;${NCBI_TREE_ROOT}/checkout_info")
    install(FILES ${_files} DESTINATION "." OPTIONAL)
endfunction()

#############################################################################
if (NOT "${NCBI_PTBCFG_INSTALL_PATH}" STREQUAL "")
    string(REPLACE "\\" "/" NCBI_PTBCFG_INSTALL_PATH ${NCBI_PTBCFG_INSTALL_PATH})
    set(CMAKE_INSTALL_PREFIX "${NCBI_PTBCFG_INSTALL_PATH}" CACHE STRING "Reset the installation destination" FORCE)
endif()
set(CMAKE_SUPPRESS_REGENERATION ON)
set(NCBI_PTBCFG_INSTALL_TAGS "*;-test;-demo;-sample")
set_property(GLOBAL PROPERTY NCBI_PTBPROP_EXPORTED_TARGETS "")
NCBI_register_hook(TARGET_ADDED NCBI_internal_install_target)
NCBI_register_hook(ALL_ADDED    NCBI_internal_install_root)
