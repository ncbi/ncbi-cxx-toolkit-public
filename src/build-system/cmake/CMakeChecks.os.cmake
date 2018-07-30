#
# OS-specific settings
#

if (UNIX)
    SET(NCBI_OS_UNIX 1 CACHE INTERNAL "Is Unix")
    SET(NCBI_OS "UNIX" CACHE INTERNAL "Is Unix")
    set(HOST_OS "unix")
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

#        set(HOST_CPU "x86_64")
        set(HOST_CPU ${CMAKE_SYSTEM_PROCESSOR})
    endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
endif(UNIX)

if (WIN32)
    SET(NCBI_OS_MSWIN 1 CACHE INTERNAL "Is Windows")
    SET(NCBI_OS "WINDOWS" CACHE INTERNAL "Is Windows")
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(HOST_OS "pc-Win32")
    else()
        set(HOST_OS "pc-x64")
    endif()
    set(HOST_OS_WITH_VERSION ${HOST_OS})
#    set(HOST_CPU "i386")
   set(HOST_CPU ${CMAKE_SYSTEM_PROCESSOR})
endif(WIN32)

if (CYGWIN)
    SET(NCBI_OS_CYGWIN 1 CACHE INTERNAL "Is CygWin")
    SET(NCBI_OS "CYGWIN" CACHE INTERNAL "Is Cygwin")
endif(CYGWIN)

if (APPLE)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        SET(NCBI_OS_DARWIN 1 CACHE INTERNAL "Is Mac OS X")
    endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
endif(APPLE)

