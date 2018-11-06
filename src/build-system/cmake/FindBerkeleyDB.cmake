# Find Berkley DB

if (APPLE)
    set(BUILD_PREFIX "Clang320-")
endif (APPLE)

set(BerkeleyDB_FOUND NO)

FIND_PATH( BERKELEYDB_INCLUDE_DIR db.h 
    PATHS ${NCBI_TOOLS_ROOT}/BerkeleyDB/include/
          /usr/local/include
          /usr/include
    NO_DEFAULT_PATH
    )

IF (BERKELEYDB_INCLUDE_DIR)
    FIND_LIBRARY( BERKELEYDB_LIBRARY NAMES db
        PATHS ${CMAKE_PREFIX_PATH}
              "${NCBI_TOOLS_ROOT}/BerkeleyDB/${BUILD_PREFIX}${CMAKE_BUILD_TYPE}/"
              /usr/local/lib
              /usr/lib
        NO_DEFAULT_PATH
        )

    IF (BERKELEYDB_LIBRARY)
        set(BerkeleyDB_FOUND Yes)
        get_filename_component(BERKELEYDB_LIBDIR ${BERKELEYDB_LIBRARY} DIRECTORY)
        get_filename_component(BERKELEYDB_LIBNAME ${BERKELEYDB_LIBRARY} NAME)
        get_filename_component(BERKELEYDB_LIBDIR ${BERKELEYDB_LIBDIR} REALPATH)
        set(BERKELEYDB_LIBRARY ${BERKELEYDB_LIBDIR}/${BERKELEYDB_LIBNAME})

        set(BERKELEYDB_LIBRARIES ${BERKELEYDB_LIBRARY} -Wl,-rpath,${BERKELEYDB_LIBDIR})
    ELSE (BERKELEYDB_LIBRARY)
        MESSAGE(WARNING "Include ${BERKELEYDB_INCLUDE}/db.h found, but no library in ${NCBI_TOOLS_ROOT}/BerkeleyDB/${BUILD_PREFIX}${CMAKE_BUILD_TYPE}/ ")
    ENDIF (BERKELEYDB_LIBRARY)

    MESSAGE(STATUS "Found BerkeleyDB: ${BERKELEYDB_LIBRARY}")
    MESSAGE(STATUS "      BerkeleyDB Libraries: ${BERKELEYDB_LIBRARIES}")
    MESSAGE(STATUS "      BerkeleyDB INCLUDE: ${BERKELEYDB_INCLUDE_DIR}")
ENDIF (BERKELEYDB_INCLUDE_DIR)
