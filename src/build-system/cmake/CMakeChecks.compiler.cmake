#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper
##  Build settings

set(NCBI_DEFAULT_PCH "ncbi_pch.hpp")
set(NCBI_DEFAULT_HEADERS "*.h*;*impl/*.h*;*.inl;*impl/*.inl")

if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
if (BUILD_SHARED_LIBS)
    set(NCBI_DLL_BUILD 1)
    set(NCBI_DLL_SUPPORT 1)
endif()

# ---------------------------------------------------------------------------
# compiler id
if (WIN32)
    set(NCBI_COMPILER_MSVC 1)
    set(NCBI_COMPILER ${CMAKE_CXX_COMPILER_ID})
    set(NCBI_COMPILER_VERSION ${MSVC_VERSION})
else()
    set(NCBI_COMPILER ${CMAKE_CXX_COMPILER_ID})
    set(_tmp ${CMAKE_CXX_COMPILER_VERSION})
    string(REPLACE "." ";" _tmp "${_tmp}")
    list(GET _tmp 0 _v1)
    list(GET _tmp 1 _v2)
    list(GET _tmp 2 _v3)
    if("${NCBI_COMPILER}" STREQUAL "IntelLLVM" AND ${_v1} GREATER 2000)
        math(EXPR _v1 "${_v1} - 2000")
    endif()
    set(NCBI_COMPILER_VERSION ${_v1}${_v2}${_v3})
    set(NCBI_COMPILER_VERSION_DOTTED ${_v1}.${_v2}.${_v3})

    if ("${NCBI_COMPILER}" STREQUAL "GNU")
        set(NCBI_COMPILER_GCC 1)
        set(NCBI_COMPILER "GCC")
    elseif ("${NCBI_COMPILER}" STREQUAL "Intel"
            OR "${NCBI_COMPILER}" STREQUAL "IntelLLVM")
        set(NCBI_COMPILER_ICC 1)
        set(NCBI_COMPILER "ICC")
        if("${NCBI_COMPILER_VERSION}" STREQUAL "1900")
            set(NCBI_COMPILER_COMPONENTS "ICC1903")
        endif()
    elseif ("${NCBI_COMPILER}" STREQUAL "AppleClang")
        set(NCBI_COMPILER_APPLE_CLANG 1)
        set(NCBI_COMPILER "APPLE_CLANG")
        set(NCBI_COMPILER_COMPONENTS "Clang${NCBI_COMPILER_VERSION};Clang")
    elseif ("${NCBI_COMPILER}" STREQUAL "Clang")
        set(NCBI_COMPILER_LLVM_CLANG 1)
        set(NCBI_COMPILER "LLVM_CLANG")
        set(NCBI_COMPILER_COMPONENTS "GCC1220")
    else()
        set(NCBI_COMPILER_UNKNOWN 1)
    endif()
endif()

# ---------------------------------------------------------------------------
# build configurations
if(DEFINED NCBI_PTBCFG_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "${NCBI_PTBCFG_CONFIGURATION_TYPES}" CACHE STRING "Reset the configurations" FORCE)
endif()

if (WIN32 OR XCODE)
    list(LENGTH CMAKE_CONFIGURATION_TYPES _count)
    if ("${CMAKE_BUILD_TYPE}" STREQUAL "" AND NOT "${_count}" EQUAL "1" AND NOT NCBI_PTBCFG_PACKAGING AND NOT NCBI_PTBCFG_PACKAGED)
        if (BUILD_SHARED_LIBS)
            set(CMAKE_CONFIGURATION_TYPES DebugDLL ReleaseDLL)
        else()
            if(CfgMT IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
                set(CMAKE_CONFIGURATION_TYPES DebugDLL ReleaseDLL DebugMT ReleaseMT)
            else()
                set(CMAKE_CONFIGURATION_TYPES DebugDLL ReleaseDLL)
            endif()
        endif()
        set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "Reset the configurations" FORCE)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "")
    endif()
    if (NOT "${CMAKE_BUILD_TYPE}" STREQUAL "")
        set(NCBI_CONFIGURATION_TYPES ${CMAKE_BUILD_TYPE})
    elseif (NOT "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "")
        set(NCBI_CONFIGURATION_TYPES ${CMAKE_CONFIGURATION_TYPES})
    endif()
    list(LENGTH NCBI_CONFIGURATION_TYPES NCBI_CONFIGURATION_TYPES_COUNT)
    if("${NCBI_CONFIGURATION_TYPES_COUNT}" EQUAL 1)
        if(WIN32)
            if ("${CMAKE_MSVC_RUNTIME_LIBRARY}" STREQUAL "MultiThreaded" OR "${CMAKE_MSVC_RUNTIME_LIBRARY}" STREQUAL "MultiThreadedDebug"
                OR    "${MSVC_RUNTIME_LIBRARY}" STREQUAL "MultiThreaded" OR       "${MSVC_RUNTIME_LIBRARY}" STREQUAL "MultiThreadedDebug")
                set(NCBI_CONFIGURATION_RUNTIMELIB "MT")
            else()
                set(NCBI_CONFIGURATION_RUNTIMELIB "DLL")
            endif()
        else()
            set(NCBI_CONFIGURATION_RUNTIMELIB "DLL")
        endif()
        NCBI_util_Cfg_ToStd(${NCBI_CONFIGURATION_TYPES} STD_BUILD_TYPE)
    else()
        set(NCBI_CONFIGURATION_RUNTIMELIB "")
        set(STD_BUILD_TYPE "Multiple")
    endif()

else()
    if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
        set(CMAKE_BUILD_TYPE Debug)
    endif()

    set(NCBI_CONFIGURATION_TYPES "${CMAKE_BUILD_TYPE}")
    list(LENGTH NCBI_CONFIGURATION_TYPES NCBI_CONFIGURATION_TYPES_COUNT)
    set(NCBI_CONFIGURATION_RUNTIMELIB "")
    NCBI_util_Cfg_ToStd(${CMAKE_BUILD_TYPE} STD_BUILD_TYPE)
    set(NCBI_BUILD_TYPE "${STD_BUILD_TYPE}MT64")
endif()

# ---------------------------------------------------------------------------
# compilation features
set(NCBI_PTBCFG_KNOWN_FEATURES
    Int8GI
    Int4GI
    StrictGI
    StrictEntrezId
    StrictTaxId
    BackwardSig
    Symbols
    StaticComponents
    BinRelease
    SSE
    OpenMP
    CfgMT
    CfgProps
    UNICODE
    MaxDebug
    Coverage
    CustomRPath
)
if(NOT "${NCBI_PTBCFG_PROJECT_FEATURES}" STREQUAL "")
    string(REPLACE "," ";" NCBI_PTBCFG_PROJECT_FEATURES "${NCBI_PTBCFG_PROJECT_FEATURES}")
    string(REPLACE " " ";" NCBI_PTBCFG_PROJECT_FEATURES "${NCBI_PTBCFG_PROJECT_FEATURES}")

    set(_all)
    foreach(_f IN LISTS NCBI_PTBCFG_PROJECT_FEATURES)
        if("${_f}" STREQUAL "noSSE")
            message("WARNING: Feature noSSE is deprecated, use -SSE instead")
            list(APPEND _all -SSE)
        elseif("${_f}" STREQUAL "noOpenMP")
            message("WARNING: Feature noOpenMP is deprecated, use -OpenMP instead")
            list(APPEND _all -OpenMP)
        elseif("${_f}" STREQUAL "noUNICODE")
            message("WARNING: Feature noUNICODE is deprecated, use -UNICODE instead")
            list(APPEND _all -UNICODE)
        elseif(NOT "${_f}" STREQUAL "")
            list(APPEND _all ${_f})
        endif()
    endforeach()
    set(NCBI_PTBCFG_PROJECT_FEATURES ${_all})
endif()

set(NCBI_PTBCFG_INSTALL_SUFFIX "")
if(Int8GI IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
    add_compile_definitions(NCBI_INT8_GI)
    set(NCBI_PTBCFG_INSTALL_SUFFIX "${NCBI_PTBCFG_INSTALL_SUFFIX}Int8GI")
endif()

if(Int4GI IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
    add_compile_definitions(NCBI_INT4_GI)
    set(NCBI_PTBCFG_INSTALL_SUFFIX "${NCBI_PTBCFG_INSTALL_SUFFIX}Int4GI")
endif()

if(StrictEntrezId IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
    add_compile_definitions(NCBI_STRICT_GI NCBI_TEST_STRICT_ENTREZ_ID)
    if(StrictTaxId IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
        add_compile_definitions(NCBI_STRICT_TAX_ID)
        set(NCBI_PTBCFG_INSTALL_SUFFIX
            "${NCBI_PTBCFG_INSTALL_SUFFIX}StrictIds")
    else()
        set(NCBI_PTBCFG_INSTALL_SUFFIX
            "${NCBI_PTBCFG_INSTALL_SUFFIX}StrictEntrezId")
    endif()
elseif(StrictTaxId IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
    add_compile_definitions(NCBI_STRICT_GI NCBI_STRICT_TAX_ID)
    set(NCBI_PTBCFG_INSTALL_SUFFIX
        "${NCBI_PTBCFG_INSTALL_SUFFIX}StrictTaxId")
elseif(StrictGI IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
    add_compile_definitions(NCBI_STRICT_GI)
    set(NCBI_PTBCFG_INSTALL_SUFFIX "${NCBI_PTBCFG_INSTALL_SUFFIX}StrictGI")
endif()

if("$ENV{DEBUG_TRACE_FATAL_SIGNALS}" OR BackwardSig IN_LIST NCBI_PTBCFG_PROJECT_FEATURES )
    set(USE_LIBBACKWARD_SIG_HANDLING 1)
endif()

if(Symbols IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
    set(NCBI_PTBCFG_INSTALL_SUFFIX "${NCBI_PTBCFG_INSTALL_SUFFIX}Symbols")
endif()

if(StaticComponents IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
    set(NCBI_PTBCFG_COMPONENT_StaticComponents ON)
endif()

if(BinRelease IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
    set(NCBI_PTBCFG_COMPONENT_StaticComponents ON)
    set(NCBI_COMPONENT_NCBICRYPT_DISABLED TRUE)
    set(NCBI_COMPONENT_BACKWARD_DISABLED TRUE)
    set(NCBI_COMPONENT_UNWIND_DISABLED TRUE)
    set(NCBI_COMPONENT_PCRE_DISABLED TRUE)
    if(NOT  SSE IN_LIST NCBI_PTBCFG_PROJECT_FEATURES AND
       NOT -SSE IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
        list(APPEND NCBI_PTBCFG_PROJECT_FEATURES -SSE)
    endif()
    if(NOT  OpenMP IN_LIST NCBI_PTBCFG_PROJECT_FEATURES AND
       NOT -OpenMP IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
        list(APPEND NCBI_PTBCFG_PROJECT_FEATURES -OpenMP)
    endif()
endif()

if(CustomRPath IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
    set(NCBI_PTBCFG_CUSTOMRPATH ON)
endif()

# see also
#    CfgMT, CfgProps in WIN32
#    MaxDebug, Coverage, SSE, OpenMP in UNIX

#----------------------------------------------------------------------------
function(NCBI_util_FindToolchain _version _result)
    set(_base "${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_SYSTEM_NAME}-${NCBI_COMPILER}")
    string(TOLOWER ${_base} _base)
#message("NCBI_util_FindToolchain: looking for ${_base}-${_version}")
    string(LENGTH ${_version} _len)
    while(${_len} GREATER 0)
        string(SUBSTRING ${_version} 0 ${_len} _ver)
        set(_name "${CMAKE_CURRENT_LIST_DIR}/toolchains/${_base}-${_ver}.cmake")
        if(EXISTS ${_name})
            set(${_result} ${_name} PARENT_SCOPE)
            return()
        endif()
        math(EXPR _len "${_len} - 1")
    endwhile()
    set(_name "${CMAKE_CURRENT_LIST_DIR}/toolchains/${_base}.cmake")
    if(EXISTS ${_name})
        set(${_result} ${_name} PARENT_SCOPE)
    else()
        set(${_result} "" PARENT_SCOPE)
    endif()
endfunction()
NCBI_util_FindToolchain(${NCBI_COMPILER_VERSION} _result)

#----------------------------------------------------------------------------
if (WIN32)

    set(NCBI_DEFAULT_PCH_DEFINE "NCBI_USE_PCH")
    set(NCBI_DEFAULT_RESOURCES "${NCBI_TREE_CMAKECFG}/ncbi.rc")
    set(NCBI_DEFAULT_DLLENTRY  "${NCBI_TREE_CMAKECFG}/dll_main.cpp")
    set(NCBI_DEFAULT_GUIENTRY  "${NCBI_TREE_CMAKECFG}/winmain.cpp")

    if (NOT NCBI_PTBCFG_FLAGS_DEFINED AND NOT NCBI_PTBCFG_PACKAGING AND NOT NCBI_PTBCFG_PACKAGED)
        if(CfgProps IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)

            if (NOT DEFINED NCBI_DEFAULT_CFGPROPS)
                set(NCBI_DEFAULT_CFGPROPS "ncbi_cpp.cfg.props")
                if (EXISTS "${NCBI_TREE_CMAKECFG}/${NCBI_DEFAULT_CFGPROPS}")
                    if (NOT EXISTS "${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}/${NCBI_DEFAULT_CFGPROPS}")
                        file(COPY "${NCBI_TREE_CMAKECFG}/${NCBI_DEFAULT_CFGPROPS}" DESTINATION "${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}")
                    endif()
                    set(NCBI_DEFAULT_CFGPROPS "${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}/${NCBI_DEFAULT_CFGPROPS}")
                else()
                    message(WARNING "File not found:  ${NCBI_TREE_CMAKECFG}/${NCBI_DEFAULT_CFGPROPS}")
                    unset(NCBI_DEFAULT_CFGPROPS)
                endif()
            endif()

            set(CMAKE_CXX_FLAGS   "/DWIN32 /D_WINDOWS /EHsc")
            set(CMAKE_C_FLAGS   "/DWIN32 /D_WINDOWS")

            set(CMAKE_CXX_FLAGS_DEBUGDLL   "/MDd")
            set(CMAKE_CXX_FLAGS_DEBUGMT    "/MTd")
            set(CMAKE_CXX_FLAGS_RELEASEDLL "/MD")
            set(CMAKE_CXX_FLAGS_RELEASEMT  "/MT")

            set(CMAKE_C_FLAGS_DEBUGDLL   "/MDd")
            set(CMAKE_C_FLAGS_DEBUGMT    "/MTd")
            set(CMAKE_C_FLAGS_RELEASEDLL "/MD")
            set(CMAKE_C_FLAGS_RELEASEMT  "/MT")
            add_compile_options(/MP /Zi)

            set(CMAKE_EXE_LINKER_FLAGS_DEBUGDLL   "/DEBUG")
            set(CMAKE_EXE_LINKER_FLAGS_DEBUGMT    "/DEBUG")

            set(CMAKE_SHARED_LINKER_FLAGS_DEBUGDLL   "/DEBUG")
            set(CMAKE_SHARED_LINKER_FLAGS_DEBUGMT    "/DEBUG")

        else()
            set(CMAKE_CXX_FLAGS_DEBUGDLL   "/MDd /Od /RTC1 /D_DEBUG")
            set(CMAKE_CXX_FLAGS_DEBUGMT    "/MTd /Od /RTC1 /D_DEBUG")
            set(CMAKE_CXX_FLAGS_RELEASEDLL "/MD  /O2 /Ob1 /DNDEBUG")
            set(CMAKE_CXX_FLAGS_RELEASEMT  "/MT  /O2 /Ob1 /DNDEBUG")

            set(CMAKE_C_FLAGS_DEBUGDLL   "/MDd /Od /RTC1 /D_DEBUG")
            set(CMAKE_C_FLAGS_DEBUGMT    "/MTd /Od /RTC1 /D_DEBUG")
            set(CMAKE_C_FLAGS_RELEASEDLL "/MD  /O2 /Ob1 /DNDEBUG")
            set(CMAKE_C_FLAGS_RELEASEMT  "/MT  /O2 /Ob1 /DNDEBUG")
            add_compile_options(/MP /Zi)

            set(CMAKE_EXE_LINKER_FLAGS_DEBUGDLL   "/DEBUG")
            set(CMAKE_EXE_LINKER_FLAGS_DEBUGMT    "/DEBUG")
            set(CMAKE_EXE_LINKER_FLAGS_RELEASEDLL "")
            set(CMAKE_EXE_LINKER_FLAGS_RELEASEMT  "")

            set(CMAKE_SHARED_LINKER_FLAGS_DEBUGDLL   "/DEBUG")
            set(CMAKE_SHARED_LINKER_FLAGS_DEBUGMT    "/DEBUG")
            set(CMAKE_SHARED_LINKER_FLAGS_RELEASEDLL "")
            set(CMAKE_SHARED_LINKER_FLAGS_RELEASEMT  "")
            add_link_options(/INCREMENTAL:NO)
        endif()

        if(Symbols IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
            set(CMAKE_EXE_LINKER_FLAGS "/DEBUG ${CMAKE_EXE_LINKER_FLAGS}")
            set(CMAKE_SHARED_LINKER_FLAGS "/DEBUG ${CMAKE_SHARED_LINKER_FLAGS}")
        endif()
        if(SSE IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
            add_compile_options("/arch:AVX2")
        endif()
    endif()

    add_compile_definitions(_CRT_SECURE_NO_WARNINGS=1)
    if(NOT -UNICODE IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
        add_compile_definitions(_UNICODE)
    endif()
    if(BUILD_SHARED_LIBS)
        add_compile_definitions(NCBI_DLL_BUILD)
    endif()

    if(DEFINED NCBI_PTBCFG_MAPPED_SOURCE)
        string(REPLACE "/" "\\" _root ${NCBI_TREE_ROOT})
        string(REPLACE "/" "\\" NCBI_PTBCFG_MAPPED_SOURCE ${NCBI_PTBCFG_MAPPED_SOURCE})
        set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /wd5048 /experimental:deterministic /pathmap:${_root}=${NCBI_PTBCFG_MAPPED_SOURCE}")
        set(CMAKE_C_FLAGS    "${CMAKE_C_FLAGS} /wd5048 /experimental:deterministic /pathmap:${_root}=${NCBI_PTBCFG_MAPPED_SOURCE}")
    endif()

    if(NOT DEFINED NCBI_DEFAULT_USEPCH)
		set(NCBI_DEFAULT_USEPCH ON)
        if ($ENV{NCBI_AUTOMATED_BUILD})
			set(NCBI_DEFAULT_USEPCH OFF)
		endif()
        string(FIND "${CMAKE_GENERATOR}" "Visual Studio" _pos)
        if(NOT ${_pos} EQUAL 0)
            set(NCBI_DEFAULT_USEPCH OFF)
        endif()
    endif()

    return()

#----------------------------------------------------------------------------
elseif (XCODE)

    find_package(Threads REQUIRED)
    if (CMAKE_USE_PTHREADS_INIT)
        add_compile_definitions(_MT _REENTRANT _THREAD_SAFE)
        set(NCBI_POSIX_THREADS 1)
    endif (CMAKE_USE_PTHREADS_INIT)

    if (NOT NCBI_PTBCFG_FLAGS_DEFINED AND NOT NCBI_PTBCFG_PACKAGING AND NOT NCBI_PTBCFG_PACKAGED)

        set(CMAKE_CXX_FLAGS_DEBUGDLL   "-g -gdwarf -ggdb3 -O0 -D_DEBUG")
        set(CMAKE_CXX_FLAGS_DEBUGMT    "-g -gdwarf -ggdb3 -O0 -D_DEBUG")
        set(CMAKE_CXX_FLAGS_RELEASEDLL "-Os -DNDEBUG")
        set(CMAKE_CXX_FLAGS_RELEASEMT  "-Os -DNDEBUG")

        set(CMAKE_C_FLAGS_DEBUGDLL   "-g -gdwarf -ggdb3 -O0 -D_DEBUG")
        set(CMAKE_C_FLAGS_DEBUGMT    "-g -gdwarf -ggdb3 -O0 -D_DEBUG")
        set(CMAKE_C_FLAGS_RELEASEDLL "-Os -DNDEBUG")
        set(CMAKE_C_FLAGS_RELEASEMT  "-Os -DNDEBUG")

        add_link_options(-stdlib=libc++ -framework CoreServices)
    endif()

    add_compile_definitions(NCBI_XCODE_BUILD _LARGEFILE_SOURCE _FILE_OFFSET_BITS=64)
    set(NCBI_DEFAULT_USEPCH ON)

    return()
endif()

#----------------------------------------------------------------------------
# UNIX
include(CheckCXXCompilerFlag)
include(CheckCCompilerFlag)

macro(NCBI_util_set_cxx_compiler_flag_optional)
    foreach (var ${ARGN})
        check_cxx_compiler_flag("${var}" _COMPILER_SUPPORTS_FLAG)
        if (_COMPILER_SUPPORTS_FLAG)
            message(STATUS "The compiler ${CMAKE_CXX_COMPILER} supports ${var}")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${var}")
            break()
        endif()
    endforeach()
    if (_COMPILER_SUPPORTS_FLAG)
    else()
        message(WARNING "The compiler ${CMAKE_CXX_COMPILER} has no support for any of ${ARGN}")
    endif()
endmacro()

macro(NCBI_util_set_c_compiler_flag_optional)
    foreach (var ${ARGN})
        check_c_compiler_flag(${var} _COMPILER_SUPPORTS_FLAG)
        if (_COMPILER_SUPPORTS_FLAG)
            message(STATUS "The compiler ${CMAKE_C_COMPILER} supports ${var}")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${var}")
            break()
        endif()
    endforeach()
    if (_COMPILER_SUPPORTS_FLAG)
    else()
        message(WARNING "The compiler ${CMAKE_C_COMPILER} has no support for any of ${ARGN}")
    endif()
endmacro()

#----------------------------------------------------------------------------

# Threading libraries
find_package(Threads REQUIRED)
if (CMAKE_USE_PTHREADS_INIT)
    add_compile_definitions(_MT _REENTRANT _THREAD_SAFE)
    set(NCBI_POSIX_THREADS 1)
endif (CMAKE_USE_PTHREADS_INIT)

if(NCBI_PTBCFG_PACKAGED)
    return()
endif()

if(NOT DEFINED CMAKE_POSITION_INDEPENDENT_CODE)
    set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
endif()

if(NOT NCBI_PTBCFG_FLAGS_DEFINED)
#----------------------------------------------------------------------------
# flags may be set by configuration scripts
    if(NOT "$ENV{NCBI_COMPILER_C_FLAGS}" STREQUAL "")
        set(NCBI_COMPILER_C_FLAGS "$ENV{NCBI_COMPILER_C_FLAGS}")
    endif()
    if(NOT "$ENV{NCBI_COMPILER_CXX_FLAGS}" STREQUAL "")
        set(NCBI_COMPILER_CXX_FLAGS "$ENV{NCBI_COMPILER_CXX_FLAGS}")
    endif()
    if(NOT "$ENV{NCBI_COMPILER_EXE_LINKER_FLAGS}" STREQUAL "")
        set(NCBI_COMPILER_EXE_LINKER_FLAGS "$ENV{NCBI_COMPILER_EXE_LINKER_FLAGS}")
    endif()
    if(NOT "$ENV{NCBI_COMPILER_SHARED_LINKER_FLAGS}" STREQUAL "")
        set(NCBI_COMPILER_SHARED_LINKER_FLAGS "$ENV{NCBI_COMPILER_SHARED_LINKER_FLAGS}")
    endif()

    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} ${NCBI_COMPILER_C_FLAGS}")
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${NCBI_COMPILER_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${NCBI_COMPILER_EXE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_SHARED_LINKER_FLAGS} ${NCBI_COMPILER_SHARED_LINKER_FLAGS}")

#----------------------------------------------------------------------------
    set(_ggdb3 "-ggdb3")
    set(_ggdb1 "-ggdb1")
    set(_gdw4r "-gdwarf-4")
    if(NCBI_COMPILER_GCC)

        if("${NCBI_COMPILER_VERSION}" LESS "700")
            add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)
        endif()

    elseif(NCBI_COMPILER_ICC)

        set(_ggdb3 "-g")
        set(_ggdb1 "")
        set(_gdw4r "")
        unset(CMAKE_POSITION_INDEPENDENT_CODE)
# Defining _GCC_NEXT_LIMITS_H ensures that <limits.h> chaining doesn't
# stop short, as can otherwise happen.
        add_compile_definitions(_GCC_NEXT_LIMITS_H)
# -we70: "incomplete type is not allowed" should be an error, not a warning!
# -wd2651: Suppress spurious "attribute does not apply to any entity"
#          when deprecating enum values (via NCBI_STD_DEPRECATED).
        if("${NCBI_COMPILER_VERSION}" GREATER 2000)
            set(CMAKE_C_FLAGS    "${CMAKE_C_FLAGS} -ffp-model=precise -fPIC")
            set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -ffp-model=precise -fPIC")
        else()
            set(CMAKE_C_FLAGS    "${CMAKE_C_FLAGS} -fPIC")
            set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -we70 -wd2651 -fPIC")
            set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -Kc++ -static-intel -diag-disable 10237")
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Kc++ -static-intel -diag-disable 10237")
        endif()
    endif()

    set(CMAKE_CXX_FLAGS_DEBUG "${_gdw4r} ${_ggdb3} -O0")
    set(CMAKE_C_FLAGS_DEBUG   "${_gdw4r} ${_ggdb3} -O0")
    if(Symbols IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
        set(CMAKE_CXX_FLAGS_RELEASE "${_gdw4r} ${_ggdb3} -O3")
        set(CMAKE_C_FLAGS_RELEASE   "${_gdw4r} ${_ggdb3} -O3")
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "${_gdw4r} ${_ggdb1} -O3")
        set(CMAKE_C_FLAGS_RELEASE   "${_gdw4r} ${_ggdb1} -O3")
    endif()

    if(CYGWIN)
        set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wa,-mbig-obj")
        set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS}  -Wa,-mbig-obj")
    endif()
    if (CMAKE_COMPILER_IS_GNUCC)
        add_compile_options(-Wall -Wno-format-y2k )
        add_link_options(-Wl,--as-needed)
    endif()

#set(CMAKE_SHARED_LINKER_FLAGS_DYNAMIC "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--export-dynamic")
    set(CMAKE_SHARED_LINKER_FLAGS_RDYNAMIC "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_ALLOW_UNDEFINED "${CMAKE_SHARED_LINKER_FLAGS}")
    if (NOT APPLE)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
    endif ()

    if (NOT WIN32 AND NOT APPLE AND NOT CYGWIN)
#this add RUNPATH to binaries (RPATH is already there anyway), which makes it more like binaries built by C++ Toolkit
        SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--enable-new-dtags")
    endif()
endif(NOT NCBI_PTBCFG_FLAGS_DEFINED)


if(Symbols IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
    if(DEFINED CMAKE_C_FLAGS_RELWITHDEBINFO)
        set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    endif()
    if(DEFINED CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        set(CMAKE_CXX_FLAGS_RELEASE   "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    endif()
endif()

if(NOT NCBI_PTBCFG_PACKAGING AND NOT -SSE IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
    if(DEFINED NCBI_COMPILER_FLAGS_SSE)
        set(CMAKE_C_FLAGS    "${CMAKE_C_FLAGS} ${NCBI_COMPILER_FLAGS_SSE}")
        set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${NCBI_COMPILER_FLAGS_SSE}")
    elseif("${HOST_CPU}" MATCHES "x86")
	    NCBI_util_set_cxx_compiler_flag_optional("-msse4.2")
	    NCBI_util_set_c_compiler_flag_optional  ("-msse4.2")
    endif()
endif()

if(Coverage IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
    if(DEFINED NCBI_COMPILER_FLAGS_COVERAGE)
        set(CMAKE_C_FLAGS    "${CMAKE_C_FLAGS} ${NCBI_COMPILER_FLAGS_COVERAGE}")
        set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${NCBI_COMPILER_FLAGS_COVERAGE}")
    elseif(CMAKE_COMPILER_IS_GNUCC)
        set(CMAKE_C_FLAGS    "${CMAKE_C_FLAGS} --coverage")
        set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} --coverage")
    endif()
    if(DEFINED NCBI_LINKER_FLAGS_COVERAGE)
        set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${NCBI_LINKER_FLAGS_COVERAGE}")
        set(CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_SHARED_LINKER_FLAGS} ${NCBI_LINKER_FLAGS_COVERAGE}")
    elseif(CMAKE_COMPILER_IS_GNUCC)
        set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} --coverage")
        set(CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_SHARED_LINKER_FLAGS} --coverage")
    endif()

endif()

if(MaxDebug IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
    add_compile_definitions(_GLIBCXX_DEBUG BOOST_USE_ASAN BOOST_USE_UCONTEXT)
    set(Boost_USE_DEBUG_RUNTIME 1)
    set(NCBI_PTBCFG_INSTALL_SUFFIX "${NCBI_PTBCFG_INSTALL_SUFFIX}MaxDebug")

    if(DEFINED NCBI_COMPILER_FLAGS_MAXDEBUG)
        set(CMAKE_C_FLAGS    "${CMAKE_C_FLAGS} ${NCBI_COMPILER_FLAGS_MAXDEBUG}")
        set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${NCBI_COMPILER_FLAGS_MAXDEBUG}")
    elseif(CMAKE_COMPILER_IS_GNUCC)
        set(CMAKE_C_FLAGS    "${CMAKE_C_FLAGS} -fstack-check -fsanitize=address")
        set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fstack-check -fsanitize=address")
    endif()
    if(DEFINED NCBI_LINKER_FLAGS_MAXDEBUG)
        set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${NCBI_LINKER_FLAGS_MAXDEBUG}")
        set(CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_SHARED_LINKER_FLAGS} ${NCBI_LINKER_FLAGS_MAXDEBUG}")
    elseif(CMAKE_COMPILER_IS_GNUCC)
        set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
        set(CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
    endif()
endif()

if(NCBI_PTBCFG_COMPONENT_StaticComponents)
    if(DEFINED NCBI_LINKER_FLAGS_STATICCOMPONENTS)
        set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${NCBI_LINKER_FLAGS_STATICCOMPONENTS}")
        set(CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_SHARED_LINKER_FLAGS} ${NCBI_LINKER_FLAGS_STATICCOMPONENTS}")
    elseif(CMAKE_COMPILER_IS_GNUCC)
        set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
        set(CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    endif()
endif()

# OpenMP
if (NOT APPLE AND NOT CYGWIN AND NOT NCBI_COMPILER_LLVM_CLANG
    AND NOT NCBI_PTBCFG_PACKAGING AND NOT NCBI_PTBCFG_PACKAGED
    AND NOT -OpenMP IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
    find_package(OpenMP)
    if (OPENMP_FOUND)
        set(CMAKE_C_FLAGS    "${CMAKE_C_FLAGS} ${OpenMP_CXX_FLAGS}")
        set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
        set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_CXX_FLAGS}")
        set(CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_SHARED_LINKER_FLAGS} ${OpenMP_CXX_FLAGS}")
    endif (OPENMP_FOUND)
endif()

if("${STD_BUILD_TYPE}" STREQUAL "Debug")
    add_compile_definitions(_DEBUG)
else()
    add_compile_definitions(NDEBUG)
endif()

if (APPLE)
    add_compile_definitions(_LARGEFILE_SOURCE _FILE_OFFSET_BITS=64)
elseif (UNIX AND NOT CYGWIN)
    add_compile_definitions(_LARGEFILE_SOURCE _LARGEFILE64_SOURCE _FILE_OFFSET_BITS=64)
endif (APPLE)

if(DEFINED NCBI_PTBCFG_MAPPED_SOURCE)
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fdebug-prefix-map=${NCBI_TREE_ROOT}=${NCBI_PTBCFG_MAPPED_SOURCE}")
    set(CMAKE_C_FLAGS    "${CMAKE_C_FLAGS} -fdebug-prefix-map=${NCBI_TREE_ROOT}=${NCBI_PTBCFG_MAPPED_SOURCE}")
endif()

#----------------------------------------------------------------------------
# RPATH
if (NOT NCBI_PTBCFG_CUSTOMRPATH)
# Establishing sane RPATH definitions
# use, i.e. don't skip the full RPATH for the build tree
    SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, use the install RPATH already
    SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

    if (APPLE)
        set(CMAKE_INSTALL_RPATH "@executable_path/../${NCBI_DIRNAME_SHARED}")
    else()
        set(CMAKE_INSTALL_RPATH "$ORIGIN/../${NCBI_DIRNAME_SHARED}")
    endif()

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
    SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    if(BinRelease IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
        set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
        set(CMAKE_INSTALL_REMOVE_ENVIRONMENT_RPATH TRUE)
    endif()
endif()

#----------------------------------------------------------------------------
# CCACHE, DISTCC
option(CMAKE_USE_CCACHE "Use 'ccache' as a preprocessor" OFF)
option(CMAKE_USE_DISTCC "Use 'distcc' as a preprocessor" OFF)

if (CMAKE_COMPILER_IS_GNUCC)
    if(Coverage IN_LIST NCBI_PTBCFG_PROJECT_FEATURES)
        set(CMAKE_USE_CCACHE OFF)
        set(CMAKE_USE_DISTCC OFF)
    endif()
endif()
if(CYGWIN)
    set(CMAKE_USE_CCACHE OFF)
    set(CMAKE_USE_DISTCC OFF)
endif()

# pass these back for ccache to pick up
if(CMAKE_USE_CCACHE)
    set(ENV{CCACHE_UMASK} 002)
    set(ENV{CCACHE_BASEDIR} ${NCBI_TREE_ROOT})
    find_program(CCACHE_EXECUTABLE ccache
                 PATHS /usr/local/ccache/4.4/bin /usr/local/ccache/3.2.5/bin/)
    if(CCACHE_EXECUTABLE)
        message(STATUS "Found CCACHE: ${CCACHE_EXECUTABLE}")
    endif()
endif()

if(CMAKE_USE_DISTCC)
    find_program(DISTCC_EXECUTABLE
                 NAMES distcc.sh distcc
                 HINTS $ENV{NCBI}/bin )
    if(DISTCC_EXECUTABLE)
        message(STATUS "Found DISTCC: ${DISTCC_EXECUTABLE}")
        set(ENV{DISTCC_FALLBACK} 0)
        set(_testdir   ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD}/CMakeFiles)
        set(_testfile  ${_testdir}/distcctest.c)
        if (EXISTS ${_testfile})
            file(REMOVE ${_testfile})
        endif()
        file(APPEND ${_testfile} "#include <stddef.h>\n")
        file(APPEND ${_testfile} "#if !defined(__GNUC__)  &&  !defined(offsetof)\n")
        file(APPEND ${_testfile} "#  define offsetof(T, F) ((size_t)((char*) &(((T*) 0)->F) - (char*) 0))\n")
        file(APPEND ${_testfile} "#endif\n")
        file(APPEND ${_testfile} "struct S { int x; };\n")
        file(APPEND ${_testfile} "int f() { return offsetof(struct S, x); }\n")
        execute_process(
            COMMAND ${DISTCC_EXECUTABLE} ${CMAKE_C_COMPILER} -c ${_testfile}
            WORKING_DIRECTORY ${_testdir}
            RESULT_VARIABLE FAILED_DISTCC
            OUTPUT_QUIET ERROR_QUIET
            )
        file(REMOVE ${_testfile})
        unset(ENV{DISTCC_FALLBACK})
        if(NOT "${FAILED_DISTCC}" STREQUAL 0)
            message("DISTCC not available for this compiler")
            set(CMAKE_USE_DISTCC OFF CACHE STRING "DISTCC not available" FORCE)
            unset(DISTCC_EXECUTABLE CACHE)
        endif()
    endif()
endif()

# See:
# http://stackoverflow.com/questions/32752446/using-compiler-prefix-commands-with-cmake-distcc-ccache

set(NCBI_COMPILER_WRAPPER "")
if (CMAKE_USE_DISTCC AND DISTCC_EXECUTABLE AND CMAKE_USE_CCACHE AND CCACHE_EXECUTABLE)
    set(NCBI_COMPILER_WRAPPER "CCACHE_BASEDIR=${NCBI_TREE_ROOT} CCACHE_PREFIX=${DISTCC_EXECUTABLE} ${CCACHE_EXECUTABLE} ${NCBI_COMPILER_WRAPPER}")
elseif (CMAKE_USE_DISTCC AND DISTCC_EXECUTABLE)
    set(NCBI_COMPILER_WRAPPER "${DISTCC_EXECUTABLE} ${NCBI_COMPILER_WRAPPER}")
elseif(CMAKE_USE_CCACHE AND CCACHE_EXECUTABLE)
    set(NCBI_COMPILER_WRAPPER "CCACHE_BASEDIR=${NCBI_TREE_ROOT} ${CCACHE_EXECUTABLE} ${NCBI_COMPILER_WRAPPER}")
endif()
message(STATUS "NCBI_COMPILER_WRAPPER = ${NCBI_COMPILER_WRAPPER}")
set(CMAKE_CXX_COMPILE_OBJECT "${NCBI_COMPILER_WRAPPER} ${CMAKE_CXX_COMPILE_OBJECT}")
set(CMAKE_C_COMPILE_OBJECT   "${NCBI_COMPILER_WRAPPER} ${CMAKE_C_COMPILE_OBJECT}")
