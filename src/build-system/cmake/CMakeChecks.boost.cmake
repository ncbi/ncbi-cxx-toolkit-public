# Boost: headers and libs [use as ${BOOST_LIBPATH} ${BOOST_*_LIBS} ${RT_LIBS}]

if (BUILD_SHARED_LIBS)
    set(Boost_USE_STATIC_LIBS       OFF)
    set(Boost_USE_STATIC_RUNTIME    OFF)
else()
    set(Boost_USE_STATIC_LIBS       ON)
    set(Boost_USE_STATIC_RUNTIME    ON)
endif()
set(Boost_USE_MULTITHREADED     ON)

#Hints for FindBoost

set(_foo_CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})

set(_boost_version "boost-1.62.0-ncbi1")

if(WIN32)
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${BOOST_ROOT})
#	set(WINDOWS_BOOST_DIR "${WIN32_PACKAGE_ROOT}/boost_1_57_0")
#	set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${WINDOWS_BOOST_DIR})
#	set(BOOST_ROOT "${WINDOWS_BOOST_DIR}")
#	set(BOOST_LIBRARYDIR "${BOOST_ROOT}/stage/lib")
else()
    # preferentially set a specific NCBI version of Boost
    set(BOOST_ROOT ${NCBI_TOOLS_ROOT}/${_boost_version})
    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release" AND
        EXISTS /opt/ncbi/64/${_boost_version}/lib/ )
        set(BOOST_LIBRARYDIR /opt/ncbi/64/${_boost_version}/lib)
    else()
        set(BOOST_LIBRARYDIR ${NCBI_TOOLS_ROOT}/${_boost_version}/lib)
    endif()
endif()

#set(Boost_DEBUG ON)
find_package(Boost
    COMPONENTS chrono context coroutine date_time filesystem
               iostreams regex serialization system thread
            )
set(CMAKE_PREFIX_PATH ${_foo_CMAKE_PREFIX_PATH})
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
