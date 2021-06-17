# Mike DiCuccio (dicuccio@ncbi.nlm.nih.gov)
#
# Find the Mongo-CXX driver
#
# This will set:
# MONGOCXX_FOUND
# MONGOCXX_INCLUDE_DIRS
# MONGOCXX_LIBRARIES
# BSONCXX_LIBRARIES
# BSONCXX_INCLUDE_DIRS
#
# Use as: ${MONGOCXX_LIBPATH} ${MONGOCXX_LIBRARIES} ${BSONCXX_LIBRARIES}

find_package(libbsoncxx REQUIRED
             PATHS $ENV{NCBI}/mongodb-3.6.3/
             NO_DEFAULT_PATH
)

find_package(libmongocxx REQUIRED
             PATHS $ENV{NCBI}/mongodb-3.6.3/
             NO_DEFAULT_PATH
)





if (libmongocxx_FOUND AND libbsoncxx_FOUND)
    set(MONGOCXX_FOUND TRUE)

    set(MONGOCXX_INCLUDE_DIRS ${LIBMONGOCXX_INCLUDE_DIRS})
    set(MONGOCXX_LIBRARIES ${LIBMONGOCXX_LIBRARIES})
    set(MONGOCXX_VERSION_STRING ${libmongocxx_VERSION})

    set(BSONCXX_INCLUDE_DIRS ${LIBBSONCXX_INCLUDE_DIRS})
    set(BSONCXX_LIBRARIES ${LIBBSONCXX_LIBRARIES})
    set(BSONCXX_VERSION_STRING ${libbsoncxx_VERSION})

    set(MONGOCXX_LIBPATH -L${LIBMONGOCXX_LIBRARY_DIRS} -Wl,-rpath,${LIBMONGOCXX_LIBRARY_DIRS})

    message(STATUS "Found MongoDB: ${MONGOCXX_LIBRARIES} ${BSONCXX_LIBRARIES} (found version \"${MONGOCXX_VERSION_STRING}\")")
else()
    set(MONGOCXX_FOUND FALSE)
    message(STATUS "Cannot find MongoDB")
endif()


