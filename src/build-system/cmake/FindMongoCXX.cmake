# Mike DiCuccio (dicuccio@ncbi.nlm.nih.gov)
#
# Find the Mongo-CXX driver
#
# This will set:
# MONGOCXX_FOUND
# MONGOCXX_INCLUDE_DIR
# MONGOCXX_LIBRARIES
# BSONCXX_LIBRARIES
# BSONCXX_INCLUDE_DIR

find_package(PkgConfig)

set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:$ENV{NCBI}/mongodb-3.0.2/lib/pkgconfig:$ENV{NCBI}/mongo-c-driver-1.4.1/lib/pkgconfig/")
#message(STATUS "PKG_CONFIG_PATH = $ENV{PKG_CONFIG_PATH}")

pkg_check_modules(_MONGOCXX REQUIRED libmongocxx)
pkg_check_modules(_BSONCXX REQUIRED libbsoncxx)

if (_MONGOCXX_FOUND)
    set(MONGOCXX_FOUND TRUE)
    set(MONGOCXX_INCLUDE_DIRS ${_MONGOCXX_INCLUDE_DIRS})
    set(MONGOCXX_LIBRARY_DIR ${_MONGOCXX_PREFIX})
    if (BUILD_SHARED_LIBS)
        set(MONGOCXX_LIBRARIES ${_MONGOCXX_LDFLAGS})
    else()
        set(MONGOCXX_LIBRARIES ${_MONGOCXX_STATIC_LDFLAGS})
    endif()
    set(MONGOCXX_VERSION_STRING ${_MONGOCXX_VERSION})
endif()

if (_BSONCXX_FOUND)
    set(BSONCXX_FOUND TRUE)
    set(BSONCXX_INCLUDE_DIRS ${_BSONCXX_INCLUDE_DIRS})
    set(BSONCXX_LIBRARY_DIR ${_BSONCXX_PREFIX})
    if (BUILD_SHARED_LIBS)
        set(BSONCXX_LIBRARIES ${_BSONCXX_LDFLAGS})
    else()
        set(BSONCXX_LIBRARIES ${_BSONCXX_STATIC_LDFLAGS})
    endif()
    set(BSONCXX_VERSION_STRING ${_BSONCXX_VERSION})
endif()


if (MONGOCXX_INCLUDE_DIRS AND BSONCXX_INCLUDE_DIRS)
    set(MONGOCXX_FOUND TRUE)
    message(STATUS "Found MongoDB: ${_MONGOCXX_LIBRARIES} (found version \"${MONGOCXX_VERSION_STRING}\")")
else()
    set(MONGOCXX_FOUND FALSE)
    message(STATUS "Cannot find MongoDB")
endif()


