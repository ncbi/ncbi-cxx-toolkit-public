# Boost: headers and libs [use as ${BOOST_LIBPATH} ${BOOST_*_LIBS} ${RT_LIBS}]

if (BUILD_SHARED_LIBS)
    set(Boost_USE_STATIC_LIBS       OFF)
    set(Boost_USE_STATIC_RUNTIME    OFF)
    add_definitions(-DBOOST_LOG_DYN_LINK)
else()
    set(Boost_USE_STATIC_LIBS       ON)
    set(Boost_USE_STATIC_RUNTIME    ON)
endif()
set(Boost_USE_MULTITHREADED     ON)

#Hints for FindBoost

set(_foo_CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})

if(WIN32)
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${BOOST_ROOT})
#	set(WINDOWS_BOOST_DIR "${WIN32_PACKAGE_ROOT}/boost_1_57_0")
#	set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${WINDOWS_BOOST_DIR})
#	set(BOOST_ROOT "${WINDOWS_BOOST_DIR}")
#	set(BOOST_LIBRARYDIR "${BOOST_ROOT}/stage/lib")
else()
	set(BOOST_ROOT ${NCBI_TOOLS_ROOT}/boost-1.57.0-ncbi1 )
endif()


#set(Boost_DEBUG ON)
#include(FindBoost)
#set(CMAKE_LIBRARY_PATH ${NCBI_TOOLS_ROOT}/boost-1.41.0/lib)
find_package(Boost
             COMPONENTS filesystem regex system
             REQUIRED)

set(BOOST_INCLUDE ${Boost_INCLUDE_DIRS})
set(BOOST_LIBPATH -L${Boost_LIBRARY_DIRS} -Wl,-rpath,${Boost_LIBRARY_DIRS})

set(CMAKE_PREFIX_PATH ${_foo_CMAKE_PREFIX_PATH})
