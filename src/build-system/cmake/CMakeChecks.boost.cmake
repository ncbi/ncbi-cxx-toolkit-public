# Boost: headers and libs [use as ${BOOST_LIBPATH} ${BOOST_*_LIBS} ${RT_LIBS}]

if (BUILD_SHARED_LIBS)
    set(Boost_USE_STATIC_LIBS       OFF)
    set(Boost_USE_STATIC_RUNTIME    OFF)
else()
    set(Boost_USE_STATIC_LIBS       ON)
    set(Boost_USE_STATIC_RUNTIME    ON)
endif()
set(Boost_USE_MULTITHREADED     ON)

if(EXISTS "${NCBI_ThirdParty_Boost}")
    set(BOOST_ROOT ${NCBI_ThirdParty_Boost})
#    set(BOOST_LIBRARYDIR ${NCBI_ThirdParty_Boost}/lib)
    set(_foo_CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})
    find_package(Boost OPTIONAL_COMPONENTS
        unit_test_framework system thread filesystem iostreams
#        coroutine program_options prg_exec_monitor test_exec_monitor
        context chrono date_time regex serialization
    )
    set(CMAKE_PREFIX_PATH ${_foo_CMAKE_PREFIX_PATH})
endif()
if(NOT Boost_FOUND)
    unset(Boost_USE_STATIC_LIBS)
    unset(Boost_USE_STATIC_RUNTIME)
    unset(BOOST_ROOT)
    find_package(Boost OPTIONAL_COMPONENTS
        unit_test_framework system thread filesystem iostreams
#        coroutine program_options prg_exec_monitor test_exec_monitor
        context chrono date_time regex serialization
    )
endif()

if(Boost_FOUND)
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
