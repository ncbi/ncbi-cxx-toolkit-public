# - find Sqlite 3
# SQLITE3_INCLUDE_DIR - Where to find Sqlite 3 header files (directory)
# SQLITE3_LIBRARIES - Sqlite 3 libraries
# SQLITE3_LIBRARY_RELEASE - Where the release library is
# SQLITE3_LIBRARY_DEBUG - Where the debug library is
# SQLITE3_FOUND - Set to TRUE if we found everything (library, includes and executable)

IF( SQLITE3_INCLUDE_DIR AND SQLITE3_LIBRARY_RELEASE AND SQLITE3_LIBRARY_DEBUG )
    SET(SQLITE3_FIND_QUIETLY TRUE)
ENDIF( SQLITE3_INCLUDE_DIR AND SQLITE3_LIBRARY_RELEASE AND SQLITE3_LIBRARY_DEBUG )

FIND_PATH( SQLITE3_INCLUDE_DIR sqlite3.h 
           PATHS /netopt/ncbi_tools/sqlite-3.6.14.2-ncbi1/include
           NO_DEFAULT_PATH )

IF (SQLITE3_INCLUDE_DIR)
    FIND_LIBRARY( SQLITE3_LIBRARY NAMES sqlite3
                  PATHS /netopt/ncbi_tools/sqlite-3.6.14.2-ncbi1/lib/
                  NO_DEFAULT_PATH )

    IF (NOT SQLITE3_LIBRARY)
        MESSAGE(FATAL_ERROR "Include ${SQLITE3_INCLUDE_DIR}/sqlite3.h found, but no library in /netopt/ncbi_tools/sqlite-3.6.14.2-ncbi1/GCC-Debug64MT/lib/ ") #XXX choose build type
    ENDIF (NOT SQLITE3_LIBRARY)
ENDIF (SQLITE3_INCLUDE_DIR)

