# Config file for NCBI-specific libxml layout
#

#############################################################################
##
## Standard boilerplate capturing NCBI library layout issues
##

if (BUILD_SHARED_LIBS)
    set(_NCBI_LIBRARY_SUFFIX .so)
    set(_NCBI_LIBRARY_STATIC )
else()
    set(_NCBI_LIBRARY_SUFFIX .a)
    set(_NCBI_LIBRARY_STATIC -static)
endif()


#############################################################################
##
## Module-specific checks
##

set(_MONGODB_VERSION "mongodb-3.6.3")
set(_MONGO_C_DRIVER_VERSION "mongo-c-driver-1.17.5")

get_filename_component(MONGOCXX_CMAKE_DIR "$ENV{NCBI}/${_MONGODB_VERSION}" REALPATH)
string(REGEX REPLACE ".*-([0-9].*)" "\\1" MONGOCXX_VERSION_STRING "${MONGOCXX_CMAKE_DIR}")

get_filename_component(MONGO_C_DRIVER_CMAKE_DIR "$ENV{NCBI}/${_MONGO_C_DRIVER_VERSION}" REALPATH)
string(REGEX REPLACE ".*-([0-9].*)" "\\1" MONGO_C_DRIVER_VERSION_STRING "${MONGO_C_DRIVER_CMAKE_DIR}")

set(MONGOCXX_FOUND True)
set(MONGOCXX_INCLUDE_DIR
    ${MONGOCXX_CMAKE_DIR}/include/mongocxx/v_noabi
    )

set(BSONCXX_FOUND True)
set(BSONCXX_INCLUDE_DIR
    ${MONGOCXX_CMAKE_DIR}/include/bsoncxx/v_noabi
    )

# Choose the proper library path
# For some libraries, we look in /opt/ncbi/64
set(_libpath_mongocxx ${MONGOCXX_CMAKE_DIR})
if (CMAKE_BUILD_TYPE STREQUAL "Release" AND BUILD_SHARED_LIBS)
    if (EXISTS /opt/ncbi/64/${_MONGODB_VERSION})
        set(_libpath_mongocxx /opt/ncbi/64/${_MONGODB_VERSION})
    endif()
endif()

set(_libpath_mongo_c_driver ${MONGO_C_DRIVER_CMAKE_DIR}/lib)
if (CMAKE_BUILD_TYPE STREQUAL "Release" AND BUILD_SHARED_LIBS)
    if (EXISTS /opt/ncbi/64/${_MONGO_C_DRIVER_VERSION}/lib)
        set(_libpath_mongo_c_driver /opt/ncbi/64/${_MONGO_C_DRIVER_VERSION}/lib)
    endif()
endif()

if (EXISTS "${_libpath_mongocxx}/${NCBI_COMPILER}${NCBI_COMPILER_VERSION}-${CMAKE_BUILD_TYPE}/lib")
    set(_libpath_mongocxx "${_libpath_mongocxx}/${NCBI_COMPILER}${NCBI_COMPILER_VERSION}-${CMAKE_BUILD_TYPE}/lib")
else()
    set(_libpath_mongocxx "${_libpath_mongocxx}/lib")
endif()

set(MONGOCXX_LIBRARIES
    ${_libpath_mongocxx}/libmongocxx${_NCBI_LIBRARY_SUFFIX}
    ${_libpath_mongo_c_driver}/libmongoc${_NCBI_LIBRARY_STATIC}-1.0${_NCBI_LIBRARY_SUFFIX}
    )

if (NOT BUILD_SHARED_LIBS)
    set(MONGOCXX_LIBRARIES ${MONGOCXX_LIBRARIES} /lib64/libresolv-2.17.so)
endif()


set(BSONCXX_LIBRARIES
    ${_libpath_mongocxx}/libbsoncxx${_NCBI_LIBRARY_SUFFIX}
    ${_libpath_mongo_c_driver}/libbson${_NCBI_LIBRARY_STATIC}-1.0${_NCBI_LIBRARY_SUFFIX}
    )

set(MONGOCXX_INCLUDE_DIRS ${MONGOCXX_INCLUDE_DIR})
set(BSONCXX_INCLUDE_DIRS ${BSONCXX_INCLUDE_DIR})

#############################################################################
##
## Logging
##

#if (_NCBI_MODULE_DEBUG)
if (True)
    message(STATUS "MONGOCXX (NCBI): FOUND = ${MONGOCXX_FOUND}")
    message(STATUS "MONGOCXX (NCBI): INCLUDE = ${MONGOCXX_INCLUDE_DIR}")
    message(STATUS "MONGOCXX (NCBI): LIBRARIES = ${MONGOCXX_LIBRARIES}")
    message(STATUS "BSONCXX (NCBI): FOUND = ${BSONCXX_FOUND}")
    message(STATUS "BSONCXX (NCBI): INCLUDE = ${BSONCXX_INCLUDE_DIR}")
    message(STATUS "BSONCXX (NCBI): LIBRARIES = ${BSONCXX_LIBRARIES}")
endif()

