#
# OS-specific settings
#

set(HOST_OS_DISTR "")

if (UNIX)
    SET(NCBI_OS_UNIX 1 CACHE INTERNAL "Is Unix")
    SET(NCBI_OS "UNIX" CACHE INTERNAL "Is Unix")
    set(HOST_OS "unix")
    if (CYGWIN)
        SET(NCBI_OS_CYGWIN 1 CACHE INTERNAL "Is CygWin")
        set(HOST_OS "cygwin")
        set(HOST_OS_WITH_VERSION "${HOST_OS}")
        set(HOST_CPU ${CMAKE_SYSTEM_PROCESSOR})
    endif()
    if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        SET(NCBI_OS_LINUX 1 CACHE INTERNAL "Is Linux")
        set(HOST_OS "linux-gnu")

        set(_tmp ${CMAKE_SYSTEM_VERSION})
        string(REPLACE "." ";" _tmp "${_tmp}")
        string(REPLACE "-" ";" _tmp "${_tmp}")
        list(GET _tmp 0 _v1)
        list(GET _tmp 1 _v2)
        list(GET _tmp 2 _v3)
        set(HOST_OS_WITH_VERSION "linux${_v1}.${_v2}.${_v3}-gnu")

        # Detect Linux distributive
        set(_distr)
        if(EXISTS /usr/bin/lsb_release)
            execute_process(
                COMMAND /usr/bin/lsb_release -is
                RESULT_VARIABLE _retcode
                OUTPUT_VARIABLE _tmp
                )
            if (_retcode EQUAL 0)
               string(REPLACE "\n" "" _distr ${_tmp})
            endif()
        elseif(EXISTS /etc/SuSE-release)
            set(_distr suse)
        elseif(EXISTS /etc/redhat-release)
            execute_process(
                COMMAND cut -d' ' -f1 /etc/redhat-release
                RESULT_VARIABLE _retcode
                OUTPUT_VARIABLE _tmp
                )
            if (_retcode EQUAL 0)
               string(REPLACE "\n" "" _distr ${_tmp})
            endif()
        elseif(EXISTS /usr/share/doc/ubuntu-keyring)
            set(_distr ubuntu)
        endif()
        if(NOT "${_distr}" STREQUAL "")
            string(TOLOWER "${_distr}" HOST_OS_DISTR)
        endif()

#        set(HOST_CPU "x86_64")
        set(HOST_CPU ${CMAKE_SYSTEM_PROCESSOR})
    endif()
endif(UNIX)

if (WIN32)
    SET(NCBI_OS_MSWIN 1 CACHE INTERNAL "Is Windows")
#    SET(NCBI_OS "WINDOWS" CACHE INTERNAL "Is Windows")
    SET(NCBI_OS ${CMAKE_SYSTEM_NAME})
    set(HOST_OS ${CMAKE_SYSTEM_NAME})
#    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
#        set(HOST_OS "pc-Win32")
#    else()
#        set(HOST_OS "pc-x64")
#    endif()
#    set(HOST_OS_WITH_VERSION ${HOST_OS})
#    set(HOST_OS_WITH_VERSION ${HOST_OS}.${CMAKE_SYSTEM_VERSION})
    set(HOST_OS_WITH_VERSION ${CMAKE_SYSTEM_NAME}.${CMAKE_SYSTEM_VERSION})
#    set(HOST_CPU "i386")
    set(HOST_CPU ${CMAKE_SYSTEM_PROCESSOR})
#    set(HOST_VENDOR "pc")
endif(WIN32)

if (APPLE)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        SET(NCBI_OS_DARWIN 1 CACHE INTERNAL "Is Mac OS X")
    endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

    SET(NCBI_OS ${CMAKE_SYSTEM_NAME})
    set(HOST_OS ${CMAKE_SYSTEM_NAME})
    set(HOST_OS_WITH_VERSION ${CMAKE_SYSTEM_NAME}.${CMAKE_SYSTEM_VERSION})
    set(HOST_CPU ${CMAKE_SYSTEM_PROCESSOR})
endif(APPLE)

if("${HOST_OS_DISTR}" STREQUAL "")
    set(HOST_OS_DISTR ${HOST_OS})
endif()
