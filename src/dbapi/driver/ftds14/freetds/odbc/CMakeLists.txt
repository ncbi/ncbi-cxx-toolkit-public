# $Id$

if(MSVC OR XCODE)
    NCBI_requires(ODBC)
else()
    NCBI_requires(XODBC)
endif()
NCBI_add_library(odbc14)
NCBI_add_subdirectory(unittests)

