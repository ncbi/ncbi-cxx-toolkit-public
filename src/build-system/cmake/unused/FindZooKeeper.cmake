set(ZooKeeper_FOUND NO)

FIND_PATH( ZOOKEEPER_INCLUDE_DIR zookeeper.h 
    PATHS /usr/include/zookeeper
    )

IF (ZOOKEEPER_INCLUDE_DIR)
    FIND_LIBRARY( ZOOKEEPER_LIBRARY NAMES libzookeeper_mt.a
        PATHS /usr/lib
              /usr/lib64
        )

    IF (ZOOKEEPER_LIBRARY)
        set(ZooKeeper_FOUND Yes)
        get_filename_component(ZOOKEEPER_LIBDIR ${ZOOKEEPER_LIBRARY} DIRECTORY)
        get_filename_component(ZOOKEEPER_LIBNAME ${ZOOKEEPER_LIBRARY} NAME)
        get_filename_component(ZOOKEEPER_LIBDIR ${ZOOKEEPER_LIBDIR} REALPATH)
        set(ZOOKEEPER_LIBRARY ${ZOOKEEPER_LIBDIR}/${ZOOKEEPER_LIBNAME})

        set(ZOOKEEPER_LIBRARIES ${ZOOKEEPER_LIBRARY} -Wl,-rpath,${ZOOKEEPER_LIBDIR} -pthread)
    ELSE (ZOOKEEPER_LIBRARY)
        MESSAGE(WARNING "Include ${ZOOKEEPER_INCLUDE}/zookeeper.h found, but no library")
    ENDIF (ZOOKEEPER_LIBRARY)

    MESSAGE(STATUS "Found ZooKeeper: ${ZOOKEEPER_LIBRARY}")
    MESSAGE(STATUS "      ZooKeeper Libraries: ${ZOOKEEPER_LIBRARIES}")
    MESSAGE(STATUS "      ZooKeeper INCLUDE: ${ZOOKEEPER_INCLUDE_DIR}")
ENDIF (ZOOKEEPER_INCLUDE_DIR)
