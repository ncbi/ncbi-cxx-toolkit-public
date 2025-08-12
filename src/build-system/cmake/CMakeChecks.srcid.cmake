#############################################################################
# $Id$
#############################################################################
##
##  NCBI CMake wrapper: Get all kinds of source tree IDs
##                      update ncbi_revision.h
##  NOTE: this script is called both during configuration and build
##        in the latter case (NCBITK_BUILDTIME), some checks are disabled
##
##    Author: Andrei Gourianov, gouriano@ncbi
##


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
        Subversion_WC_INFO(${_tmp} ${_prefix} IGNORE_SVN_FAILURE)
    else()
        Subversion_WC_INFO(${_dir} ${_prefix} IGNORE_SVN_FAILURE)
    endif()
endmacro()

#############################################################################
# Stable components
#
set(NCBI_CPP_TOOLKIT_VERSION_MAJOR 29)
set(NCBI_CPP_TOOLKIT_VERSION_MINOR 0)
set(NCBI_CPP_TOOLKIT_VERSION_PATCH 0)
set(NCBI_CPP_TOOLKIT_VERSION_EXTRA "")
set(NCBI_CPP_TOOLKIT_VERSION
    ${NCBI_CPP_TOOLKIT_VERSION_MAJOR}.${NCBI_CPP_TOOLKIT_VERSION_MINOR}.${NCBI_CPP_TOOLKIT_VERSION_PATCH}${NCBI_CPP_TOOLKIT_VERSION_EXTRA})

#############################################################################
# Version control systems
#
if (DEFINED NCBI_EXTERNAL_TREE_ROOT)
    set(_tree_root ${NCBI_TREE_ROOT})
else()
    set(_tree_root ${NCBITK_TREE_ROOT})
endif()

if(EXISTS ${_tree_root}/.git)
    include(FindGit)
endif()
if (GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} -C ${_tree_root} log -1 --format=%h
        OUTPUT_VARIABLE TOOLKIT_GIT_REVISION ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} -C ${_tree_root} branch --show-current
        OUTPUT_VARIABLE TOOLKIT_GIT_BRANCH ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
if(NOT "${TOOLKIT_GIT_REVISION}" STREQUAL "")
    message(STATUS "Git revision = ${TOOLKIT_GIT_REVISION}")
endif()

if(NOT EXISTS ${_tree_root}/.git)
    include(FindSubversion)
endif()
set(TOOLKIT_WC_REVISION 0)
if(NOT "$ENV{SVNREV}" STREQUAL "")
    set(TOOLKIT_WC_REVISION "$ENV{SVNREV}")
    set(TOOLKIT_WC_URL "$ENV{SVNURL}")
elseif (Subversion_FOUND)
    NCBI_Subversion_WC_INFO(${_tree_root} TOOLKIT)
else()
    set(TOOLKIT_WC_URL "")
endif()
if(NOT "$ENV{NCBI_SUBVERSION_REVISION}" STREQUAL "")
    set(TOOLKIT_WC_REVISION "$ENV{NCBI_SUBVERSION_REVISION}")
endif()
set(NCBI_SUBVERSION_REVISION ${TOOLKIT_WC_REVISION})
if(NOT "${NCBI_SUBVERSION_REVISION}" STREQUAL "0")
    message(STATUS "SVN revision = ${NCBI_SUBVERSION_REVISION}")
#    message(STATUS "SVN URL = ${TOOLKIT_WC_URL}")
endif()

if(NOT "${TOOLKIT_GIT_REVISION}" STREQUAL "")
    set(NCBI_GIT_BRANCH "${TOOLKIT_GIT_BRANCH}")
    set(NCBI_REVISION ${TOOLKIT_GIT_REVISION})
    set(HAVE_NCBI_REVISION 1)
elseif(NOT "${TOOLKIT_WC_REVISION}" STREQUAL "")
    set(NCBI_REVISION ${TOOLKIT_WC_REVISION})
    set(HAVE_NCBI_REVISION 1)
endif()

set(_tk_common_include "${NCBITK_INC_ROOT}/common")
if(NOT NCBITK_BUILDTIME)
    if (Subversion_FOUND AND EXISTS ${NCBITK_TREE_ROOT}/src/build-system/.svn)
        NCBI_Subversion_WC_INFO(${NCBITK_TREE_ROOT}/src/build-system CORELIB)
    else()
        set(CORELIB_WC_REVISION 0)
        set(CORELIB_WC_URL "")
    endif()

    if(NOT "$ENV{NCBI_SC_VERSION}" STREQUAL "")
        set(NCBI_SC_VERSION $ENV{NCBI_SC_VERSION})
    else()
        set(NCBI_SC_VERSION 0)
        if (NOT "${CORELIB_WC_URL}" STREQUAL "")
            string(REGEX REPLACE ".*/production/components/infrastructure/([0-9]+)\\.[0-9]+/.*" "\\1" _SC_VER "${CORELIB_WC_URL}")
            string(LENGTH "${_SC_VER}" _SC_VER_LEN)
            if (${_SC_VER_LEN} LESS 10 AND NOT "${_SC_VER}" STREQUAL "")
                set(NCBI_SC_VERSION ${_SC_VER})
                message(STATUS "Stable Components Number = ${NCBI_SC_VERSION}")
            endif()
        else()
            set(NCBI_SC_VERSION ${NCBI_CPP_TOOLKIT_VERSION_MAJOR})
        endif()
    endif()

    set(NCBI_TEAMCITY_BUILD_NUMBER 0)
    if (NOT "$ENV{TEAMCITY_VERSION}" STREQUAL "")
        set(NCBI_TEAMCITY_BUILD_NUMBER   $ENV{BUILD_NUMBER})
        set(NCBI_TEAMCITY_PROJECT_NAME   $ENV{TEAMCITY_PROJECT_NAME})
        set(NCBI_TEAMCITY_BUILDCONF_NAME $ENV{TEAMCITY_BUILDCONF_NAME})
        if(EXISTS "$ENV{TEAMCITY_BUILD_PROPERTIES_FILE}")
            file(STRINGS "$ENV{TEAMCITY_BUILD_PROPERTIES_FILE}" _list)
            foreach( _item IN LISTS _list)
                if ("${_item}" MATCHES "teamcity.build.id")
                    string(REPLACE "teamcity.build.id" "" _item ${_item})
                    string(REPLACE " " "" _item ${_item})
                    string(REPLACE "=" "" _item ${_item})
                    set(NCBI_TEAMCITY_BUILD_ID ${_item})
                    break()
                endif()
            endforeach()
        else()
            NCBI_notice("$ENV{TEAMCITY_BUILD_PROPERTIES_FILE} DOES NOT EXIST")
        endif()
        if ("${NCBI_TEAMCITY_BUILD_ID}" STREQUAL "")
            string(RANDOM _name)
            string(UUID NCBI_TEAMCITY_BUILD_ID NAMESPACE "73203eb4-80d3-4957-a110-8aae92c7e615" NAME ${_name} TYPE SHA1)
        endif()
        message(STATUS "TeamCity build number = ${NCBI_TEAMCITY_BUILD_NUMBER}")
        message(STATUS "TeamCity project name = ${NCBI_TEAMCITY_PROJECT_NAME}")
        message(STATUS "TeamCity build conf   = ${NCBI_TEAMCITY_BUILDCONF_NAME}")
        message(STATUS "TeamCity build ID     = ${NCBI_TEAMCITY_BUILD_ID}")
    endif()

    if(NOT DEFINED NCBI_EXTERNAL_TREE_ROOT AND EXISTS "${_tk_common_include}/ncbi_revision.h.in")
        if (WIN32 OR XCODE)
            foreach(_cfg ${NCBI_CONFIGURATION_TYPES})
                if (DEFINED NCBI_EXTERNAL_TREE_ROOT)
                    configure_file(${_tk_common_include}/ncbi_revision.h.in ${NCBI_INC_ROOT}/common/ncbi_revision.h)
                    NCBI_util_gitignore(${NCBI_INC_ROOT}/common/ncbi_revision.h)
                else()
                    if ($ENV{NCBI_AUTOMATED_BUILD})
                        configure_file(${_tk_common_include}/ncbi_revision.h.in ${NCBI_CFGINC_ROOT}/${_cfg}/common/ncbi_revision.h)
                    else()
                        configure_file(${_tk_common_include}/ncbi_revision.h.in ${NCBITK_INC_ROOT}/common/ncbi_revision.h)
                        NCBI_util_gitignore(${NCBITK_INC_ROOT}/common/ncbi_revision.h)
                    endif()
                endif()
            endforeach()
        else()
            if (DEFINED NCBI_EXTERNAL_TREE_ROOT)
                configure_file(${_tk_common_include}/ncbi_revision.h.in ${NCBI_INC_ROOT}/common/ncbi_revision.h)
                NCBI_util_gitignore(${NCBI_INC_ROOT}/common/ncbi_revision.h)
            else()
                if ($ENV{NCBI_AUTOMATED_BUILD})
                    configure_file(${_tk_common_include}/ncbi_revision.h.in ${NCBI_CFGINC_ROOT}/common/ncbi_revision.h)
                else()
                    configure_file(${_tk_common_include}/ncbi_revision.h.in ${NCBITK_INC_ROOT}/common/ncbi_revision.h)
                    NCBI_util_gitignore(${NCBITK_INC_ROOT}/common/ncbi_revision.h)
                endif()
            endif()
        endif()
    endif()
else()
    if(NOT DEFINED NCBI_EXTERNAL_TREE_ROOT AND EXISTS "${_tk_common_include}/ncbi_revision.h.in")
        configure_file(${_tk_common_include}/ncbi_revision.h.in ${NCBITK_INC_ROOT}/common/ncbi_revision.h)
    endif()
endif()
