############################################################################
#
# Compression libraries

set(_foo_CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})
if (WIN32)
    set(CMAKE_PREFIX_PATH ${ZLIB_ROOT})
endif()

find_package(ZLIB)
find_package(BZip2)
find_package(LZO)

# For backward compatibility
set(Z_INCLUDE ${ZLIB_INCLUDE_DIR})
set(BZ2_INCLUDE ${BZIP2_INCLUDE_DIR})

set(Z_LIBS ${ZLIB_LIBRARIES})
set(BZ2_LIBS ${BZIP2_LIBRARIES})

set(CMPRS_INCLUDE ${Z_INCLUDE} ${BZ2_INCLUDE})
set(CMPRS_LIBS ${Z_LIBS} ${BZ2_LIBS} ${LZO_LIBS})

if (LZO_FOUND)
	set(LZO_INCLUDE ${LZO_INCLUDE_DIR})
	set(LZO_LIBS ${LZO_LIBRARIES})
    set(HAVE_LIBLZO True)
	set(CMPRS_INCLUDE ${CMPRS_INCLUDE} ${LZO_INCLUDE})
	set(CMPRS_LIBS ${CMPRS_LIBS} ${LZO_LIBS})
endif()

set(COMPRESS_LIBS xcompress ${CMPRS_LIBS})

set(CMAKE_PREFIX_PATH ${_foo_CMAKE_PREFIX_PATH})

