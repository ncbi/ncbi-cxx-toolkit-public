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
        set(HOST_CPU "x86_64")
    endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
endif(UNIX)

if (WIN32)
    SET(NCBI_OS_MSWIN 1 CACHE INTERNAL "Is Windows")
    SET(NCBI_OS "WINDOWS" CACHE INTERNAL "Is Windows")
    set(HOST_OS "pc-x64")
    set(HOST_CPU "i386")
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


#
# Host and system definition information
#

cmake_host_system_information(RESULT _local_host_name  QUERY HOSTNAME)
set(HOST "${HOST_CPU}-unknown-${HOST_OS}")
set(NCBI_SIGNATURE "${CMAKE_C_COMPILER_ID}_${MSVC_VERSION}-${HOST}-${_local_host_name}")

if (WIN32)
    set(HOST "${HOST_CPU}-${HOST_OS}")
    set(NCBI_SIGNATURE "${CMAKE_C_COMPILER_ID}_${MSVC_VERSION}-${HOST}-${_local_host_name}")
endif()


