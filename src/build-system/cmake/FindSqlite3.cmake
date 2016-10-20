# Find Sqlite3
# ~~~~~~~~~~~~
# Try NCBI layout first

#message("FindSqlite3 SQLITE3_INCLUDE_DIR ${SQLITE3_INCLUDE_DIR}")
if (APPLE)
  set(SQLITE3_NAME "sqlite-3.6.14.2-ncbi1")
else (APPLE)
  set(SQLITE3_NAME "sqlite-3.7.13-ncbi1")
endif (APPLE)

IF( SQLITE3_INCLUDE_DIR AND SQLITE3_LIBRARY_RELEASE AND SQLITE3_LIBRARY_DEBUG )
    SET(SQLITE3_FIND_QUIETLY TRUE)
ENDIF( SQLITE3_INCLUDE_DIR AND SQLITE3_LIBRARY_RELEASE AND SQLITE3_LIBRARY_DEBUG )

FIND_PATH( SQLITE3_INCLUDE_DIR sqlite3.h 
           PATHS ${NCBI_TOOLS_ROOT}/${SQLITE3_NAME}/include
           NO_DEFAULT_PATH )

IF (SQLITE3_INCLUDE_DIR)
    FIND_LIBRARY( SQLITE3_LIBRARY NAMES sqlite3
                  PATHS ${NCBI_TOOLS_ROOT}/${SQLITE3_NAME}/${CMAKE_BUILD_TYPE}64MT/lib/
                  NO_DEFAULT_PATH )

    IF (NOT SQLITE3_LIBRARY)
      MESSAGE(FATAL_ERROR "Include ${SQLITE3_INCLUDE_DIR}/sqlite3.h found, but no library in ${NCBI_TOOLS_ROOT}/${SQLITE3_NAME}/${CMAKE_BUILD_TYPE}64MT/lib/ ")
    ELSE (NOT SQLITE3_LIBRARY)
      RETURN()
    ENDIF (NOT SQLITE3_LIBRARY)
ENDIF (SQLITE3_INCLUDE_DIR)

# ~~~~~~~~~~~~
# Copyright (c) 2007, Martin Dobias <wonder.sk at gmail.com>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# CMake module to search for Sqlite3 library
#
# If it's found it sets SQLITE3_FOUND to TRUE
# and following variables are set:
#    SQLITE3_INCLUDE_DIR
#    SQLITE3_LIBRARY


# FIND_PATH and FIND_LIBRARY normally search standard locations
# before the specified paths. To search non-standard paths first,
# FIND_* is invoked first with specified paths and NO_DEFAULT_PATH
# and then again with no specified paths to search the default
# locations. When an earlier FIND_* succeeds, subsequent FIND_*s
# searching for the same item do nothing. 

# try to use framework on mac
# want clean framework path, not unix compatibility path
IF (APPLE)
  IF (CMAKE_FIND_FRAMEWORK MATCHES "FIRST"
      OR CMAKE_FRAMEWORK_PATH MATCHES "ONLY"
      OR NOT CMAKE_FIND_FRAMEWORK)
    SET (CMAKE_FIND_FRAMEWORK_save ${CMAKE_FIND_FRAMEWORK} CACHE STRING "" FORCE)
    SET (CMAKE_FIND_FRAMEWORK "ONLY" CACHE STRING "" FORCE)
    #FIND_PATH(SQLITE3_INCLUDE_DIR SQLite3/sqlite3.h)
    FIND_LIBRARY(SQLITE3_LIBRARY SQLite3)
    IF (SQLITE3_LIBRARY)
      # FIND_PATH doesn't add "Headers" for a framework
      SET (SQLITE3_INCLUDE_DIR ${SQLITE3_LIBRARY}/Headers CACHE PATH "Path to a file.")
    ENDIF (SQLITE3_LIBRARY)
    SET (CMAKE_FIND_FRAMEWORK ${CMAKE_FIND_FRAMEWORK_save} CACHE STRING "" FORCE)
  ENDIF ()
ENDIF (APPLE)

FIND_PATH(SQLITE3_INCLUDE_DIR sqlite3.h
  "$ENV{LIB_DIR}/include"
  "$ENV{LIB_DIR}/include/sqlite"
  #mingw
  c:/msys/local/include
  NO_DEFAULT_PATH
  )
FIND_PATH(SQLITE3_INCLUDE_DIR sqlite3.h)

FIND_LIBRARY(SQLITE3_LIBRARY NAMES sqlite3 sqlite3_i PATHS
  $ENV{LIB} 
  /usr/lib 
  "$ENV{LIB_DIR}/lib"
  #mingw
  c:/msys/local/lib
  NO_DEFAULT_PATH
  )
FIND_LIBRARY(SQLITE3_LIBRARY NAMES sqlite3)

IF (SQLITE3_INCLUDE_DIR AND SQLITE3_LIBRARY)
   SET(SQLITE3_FOUND TRUE)
ENDIF (SQLITE3_INCLUDE_DIR AND SQLITE3_LIBRARY)


IF (SQLITE3_FOUND)

   IF (NOT SQLITE3_FIND_QUIETLY)
      MESSAGE(STATUS "Found Sqlite3: ${SQLITE3_LIBRARY}")
   ENDIF (NOT SQLITE3_FIND_QUIETLY)

ELSE (SQLITE3_FOUND)

   IF (SQLITE3_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find Sqlite3")
   ENDIF (SQLITE3_FIND_REQUIRED)

ENDIF (SQLITE3_FOUND)
