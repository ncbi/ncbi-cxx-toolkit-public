if(NCBI_COMPONENT_Boost_DISABLED OR NCBI_COMPONENT_Boost_FOUND)
    return()
endif()
# Boost: headers and libs
set(Boost_USE_MULTITHREADED     ON)
set(Boost_REQUESTED_COMPONENTS
    system thread filesystem iostreams
    unit_test_framework
    context chrono date_time regex serialization timer
)

if(EXISTS "${NCBI_ThirdParty_Boost}"
    AND NOT NCBI_PTBCFG_USECONAN AND NOT NCBI_PTBCFG_HASCONAN)
    if (BUILD_SHARED_LIBS)
        set(Boost_USE_STATIC_LIBS       OFF)
        set(Boost_USE_STATIC_RUNTIME    OFF)
    else()
        set(Boost_USE_STATIC_LIBS       ON)
        # set(Boost_USE_STATIC_RUNTIME    ON)
    endif()
    set(Boost_ROOT ${NCBI_ThirdParty_Boost})
    find_package(Boost OPTIONAL_COMPONENTS ${Boost_REQUESTED_COMPONENTS}
        PATHS ${NCBI_ThirdParty_Boost}
    )
    if(NOT Boost_FOUND)
        unset(Boost_USE_STATIC_LIBS)
        unset(Boost_USE_STATIC_RUNTIME)
        unset(Boost_ROOT)
    endif()
endif()
if(NOT Boost_FOUND)
    find_package(Boost OPTIONAL_COMPONENTS ${Boost_REQUESTED_COMPONENTS} NO_CMAKE_SYSTEM_PATH)
    if(NOT Boost_FOUND)
        find_package(Boost OPTIONAL_COMPONENTS ${Boost_REQUESTED_COMPONENTS})
    endif()
endif()

foreach(_inc IN ITEMS _RELEASE _DEBUG)
    if (EXISTS ${Boost_INCLUDE_DIRS${_inc}})
        set(_boost_inc ${Boost_INCLUDE_DIRS${_inc}})
        break()
    elseif (EXISTS ${boost_INCLUDE_DIRS${_inc}})
        set(_boost_inc ${boost_INCLUDE_DIRS${_inc}})
        break()
    elseif (EXISTS ${boost_PACKAGE_FOLDER${_inc}}/include)
        set(_boost_inc ${boost_PACKAGE_FOLDER${_inc}}/include)
        break()
    endif()
endforeach()

if(OFF AND Boost_FOUND)
    add_definitions(-DBOOST_LOG_DYN_LINK)

    set(BOOST_INCLUDE ${Boost_INCLUDE_DIRS})
    set(BOOST_LIBPATH -Wl,-rpath,${Boost_LIBRARY_DIRS} -L${Boost_LIBRARY_DIRS})

    message(STATUS "Boost libraries: ${Boost_LIBRARY_DIRS}")
    set(BOOST_LIBPATH -Wl,-rpath,${BOOST_LIBRARYDIR} -L${Boost_LIBRARY_DIRS})

#
# As a blanket statement, we now include Boost everywhere
# This avoids a serious insidious version skew if we have both the
# system-installed Boost libraries and a custom version of Boost
    include_directories(SYSTEM ${BOOST_INCLUDE})
endif()

#############################################################################
if (EXISTS ${Boost_INCLUDE_DIRS})
    message(STATUS "Found Boost.Test.Included: ${Boost_INCLUDE_DIRS}")
    set(NCBI_COMPONENT_Boost.Test.Included_FOUND YES)
    set(NCBI_COMPONENT_Boost.Test.Included_INCLUDE ${Boost_INCLUDE_DIRS})
    set(NCBI_COMPONENT_Boost.Test.Included_DEFINES BOOST_TEST_NO_LIB)
elseif (EXISTS ${_boost_inc})
    message(STATUS "Found Boost.Test.Included: ${_boost_inc}")
    set(NCBI_COMPONENT_Boost.Test.Included_FOUND YES)
    set(NCBI_COMPONENT_Boost.Test.Included_INCLUDE ${_boost_inc})
    set(NCBI_COMPONENT_Boost.Test.Included_DEFINES BOOST_TEST_NO_LIB)
elseif(EXISTS ${NCBI_ThirdParty_Boost}/include)
    message(STATUS "Found Boost.Test.Included: ${NCBI_ThirdParty_Boost}")
    set(NCBI_COMPONENT_Boost.Test.Included_FOUND YES)
    set(NCBI_COMPONENT_Boost.Test.Included_INCLUDE ${NCBI_ThirdParty_Boost}/include)
    set(NCBI_COMPONENT_Boost.Test.Included_DEFINES BOOST_TEST_NO_LIB)
else()
    NCBI_notice("NOT FOUND Boost.Test.Included")
    set(NCBI_COMPONENT_Boost.Test.Included_FOUND NO)
endif()

#############################################################################
if(TARGET Boost::unit_test_framework)
    message(STATUS "Found Boost.Test")
    set(NCBI_COMPONENT_Boost.Test_FOUND YES)
    set(NCBI_COMPONENT_Boost.Test_LIBS    Boost::unit_test_framework)
else()
    NCBI_notice("NOT FOUND Boost.Test")
    set(NCBI_COMPONENT_Boost.Test_FOUND NO)
endif()

#############################################################################
if(TARGET Boost::thread AND TARGET Boost::system)
    message(STATUS "Found Boost.Spirit")
    set(NCBI_COMPONENT_Boost.Spirit_FOUND YES)
    set(NCBI_COMPONENT_Boost.Spirit_LIBS    Boost::thread Boost::system)
else()
    NCBI_notice("NOT FOUND Boost.Spirit")
    set(NCBI_COMPONENT_Boost.Spirit_FOUND NO)
endif()

#############################################################################
if(TARGET Boost::thread)
    message(STATUS "Found Boost.Thread")
    set(NCBI_COMPONENT_Boost.Thread_FOUND YES)
    set(NCBI_COMPONENT_Boost.Thread_LIBS    Boost::thread)
else()
    NCBI_notice("NOT FOUND Boost.Thread")
    set(NCBI_COMPONENT_Boost.Thread_FOUND NO)
endif()

#############################################################################
foreach( _type IN ITEMS Chrono Filesystem Iostreams Regex Serialization System)
    string(TOLOWER ${_type} _lowtype)
    if(TARGET Boost::${_lowtype})
        message(STATUS "Found Boost.${_type}")
        set(NCBI_COMPONENT_Boost.${_type}_FOUND YES)
        set(NCBI_COMPONENT_Boost.${_type}_LIBS    Boost::${_lowtype})
    else()
        NCBI_notice("NOT FOUND Boost.${_type}")
        set(NCBI_COMPONENT_Boost.${_type}_FOUND NO)
    endif()
endforeach()

#############################################################################
if(Boost_FOUND)
    set(_libs)
    foreach( _lib IN LISTS Boost_REQUESTED_COMPONENTS)
        if(NOT ${_lib} MATCHES "unit_test_framework" AND TARGET Boost::${_lib})
            list(APPEND _libs Boost::${_lib})
        endif()
    endforeach()
    set(NCBI_COMPONENT_Boost_FOUND YES)
    set(NCBI_COMPONENT_Boost_LIBS ${_libs})
    if(NCBI_TRACE_COMPONENT_Boost OR NCBI_TRACE_ALLCOMPONENTS)
        message("NCBI_COMPONENT_Boost_LIBS: ${NCBI_COMPONENT_Boost_LIBS}")
    endif()
else()
    NCBI_notice("NOT FOUND Boost")
    set(NCBI_COMPONENT_Boost_FOUND NO)
endif()

