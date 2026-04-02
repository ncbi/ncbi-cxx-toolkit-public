#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI C++ Toolkit Conan package adapter
##  Installs required packages
##    Author: Andrei Gourianov, gouriano@ncbi
##


if(NCBI_PTBCFG_USECONAN)
    NCBI_notice("#############################################################################")
    NCBI_notice("Installing Conan packages")
    find_program(NCBI_CONAN_APP conan${CMAKE_EXECUTABLE_SUFFIX})
    if(NCBI_CONAN_APP)
        execute_process(
            COMMAND ${NCBI_CONAN_APP} version
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            RESULT_VARIABLE NCBI_CONAN_VERSION
            OUTPUT_QUIET ERROR_QUIET
        )
        if(NCBI_CONAN_VERSION EQUAL 0)
            set(NCBI_CONAN_VERSION 2)
        else()
            set(NCBI_CONAN_VERSION 1)
            message(FATAL_ERROR "Conan v1.x is not supported")
        endif()
        NCBI_notice("Conan: ${NCBI_CONAN_APP}")
        execute_process(
            COMMAND ${NCBI_CONAN_APP} --version
        )
    else()
        message(FATAL_ERROR "Conan not found")
    endif()

    if(EXISTS "${CMAKE_BINARY_DIR}/conanfile.py")
        file(REMOVE "${CMAKE_BINARY_DIR}/conanfile.py")
    endif()
    if(EXISTS "${CMAKE_BINARY_DIR}/conanfile.txt")
        file(REMOVE "${CMAKE_BINARY_DIR}/conanfile.txt")
    endif()
    if(DEFINED NCBI_EXTERNAL_BUILD_ROOT AND EXISTS "${NCBI_EXTERNAL_BUILD_ROOT}/${NCBI_DIRNAME_EXPORT}/conanfile.py")
        file(COPY "${NCBI_EXTERNAL_BUILD_ROOT}/${NCBI_DIRNAME_EXPORT}/conanfile.py" DESTINATION "${CMAKE_BINARY_DIR}")
        set(_conanfile "${CMAKE_BINARY_DIR}/conanfile.py")
    elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/conanfile.py")
        file(COPY "${CMAKE_CURRENT_LIST_DIR}/conanfile.py" DESTINATION "${CMAKE_BINARY_DIR}")
        set(_conanfile "${CMAKE_BINARY_DIR}/conanfile.py")
    else()
        message(FATAL_ERROR "Not found conanfile: ${CMAKE_CURRENT_LIST_DIR}/conanfile.py")
    endif()

    set(NCBI_CMAKE_APP ${CMAKE_COMMAND})
    NCBI_notice("CMake: ${NCBI_CMAKE_APP}")

    if(NOT "${NCBI_PTBCFG_CONAN_PROFILE}" STREQUAL "")
        set(_profile "${NCBI_PTBCFG_CONAN_PROFILE}")
    else()
        set(_profile default)
    endif()
    set(_cmd install ${CMAKE_BINARY_DIR} --build missing -pr:b ${_profile} -of ${CMAKE_BINARY_DIR}/${NCBI_DIRNAME_CONANGEN})
    if( NOT "${NCBI_PTBCFG_PROJECT_COMPONENTS}" STREQUAL "")
        string(REPLACE ";" "," _req "${NCBI_PTBCFG_PROJECT_COMPONENTS}")
        set(_cmd ${_cmd} -o ncbi-cxx-toolkit/*:with_req=${_req})
    endif()
    if(StaticComponents IN_LIST NCBI_PTBCFG_PROJECT_FEATURES OR
       BinRelease IN_LIST NCBI_PTBCFG_PROJECT_FEATURES OR (MSVC AND NOT BUILD_SHARED_LIBS))
        set(_cmd ${_cmd} -o *:shared=False -o:b *:shared=False)
    else()
        set(_cmd ${_cmd} -o *:shared=True -o:b *:shared=True)
        if(MSVC AND EXISTS ${_conanfile})
            file(STRINGS ${_conanfile} _options)
            foreach(_opt IN LISTS _options)
                string(FIND "${_opt}" "#" _p1)
                string(FIND "${_opt}" ":shared" _p2)
                string(FIND "${_opt}" ".shared" _p3)
                if(${_p1} LESS 0)
                    if(${_p3} GREATER 0)
                        string(REPLACE "self.options[\"" "" _opt "${_opt}")
                        string(REPLACE "\"]." ":" _opt "${_opt}")
                        string(REPLACE " " "" _opt "${_opt}")
                        set(_cmd ${_cmd} -o ${_opt} -o:b ${_opt})
                    elseif(${_p2} GREATER 0)
                        string(REPLACE " " "" _opt "${_opt}")
                        set(_cmd ${_cmd} -o ${_opt} -o:b ${_opt})
                    endif()
                endif()
            endforeach()
        endif()
    endif()

    string(REPLACE "," ";" NCBI_PTBCFG_CONAN_ARGS "${NCBI_PTBCFG_CONAN_ARGS}")
    string(REPLACE " " ";" NCBI_PTBCFG_CONAN_ARGS "${NCBI_PTBCFG_CONAN_ARGS}")
    set(_cmd ${_cmd} ${NCBI_PTBCFG_CONAN_ARGS})

    set(_types)
    if (NOT "${NCBI_PTBCFG_CONFIGURATION_TYPES}" STREQUAL "")
        set(_types ${NCBI_PTBCFG_CONFIGURATION_TYPES})
    elseif (NOT "${CMAKE_BUILD_TYPE}" STREQUAL "")
        set(_types ${CMAKE_BUILD_TYPE})
    elseif (NOT "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "")
        set(_types ${CMAKE_CONFIGURATION_TYPES})
    endif()
    set(_configs)
    foreach(_t IN LISTS _types)
        NCBI_util_Cfg_ToStd(${_t} _cfg)
        if(NOT MSVC)
            set(_cmd${_cfg} ${_cmd} -s build_type=${_cfg})
#invoke Conan install so that it will generate files for the debug configuration, pointing at the release one
#set(_cmd${_cfg} ${_cmd} -s &:build_type=${_cfg} -s build_type=Release)
        else()
            set(_cmd${_cfg} ${_cmd} -s build_type=${_cfg} -s compiler.runtime_type=${_cfg})
        endif()
        list(APPEND _configs ${_cfg})
    endforeach()
    list(REMOVE_DUPLICATES _configs)

    foreach(_cfg IN LISTS _configs)
        execute_process(
            COMMAND ${NCBI_CONAN_APP} ${_cmd${_cfg}}
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            RESULT_VARIABLE CONAN_INSTALL_RESULT
        )
        if(NOT CONAN_INSTALL_RESULT EQUAL "0")
            message(FATAL_ERROR "Conan setup failed: error = ${CONAN_INSTALL_RESULT}")
        endif()
    endforeach()

    NCBI_notice("Done with installing Conan packages")
    NCBI_notice("#############################################################################")
else()
    message(FATAL_ERROR "Incorrect include of ${CMAKE_CURRENT_LIST_FILE}")
endif()
