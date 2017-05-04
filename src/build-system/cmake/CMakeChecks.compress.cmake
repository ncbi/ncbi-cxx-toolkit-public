############################################################################
#
# Compression libraries

find_package(ZLIB)
find_package(BZip2)
find_package(LZO)

# For backward compatibility
set(Z_INCLUDE ${ZLIB_INCLUDE_DIR})
set(BZ2_INCLUDE ${BZIP2_INCLUDE_DIR})
set(LZO_INCLUDE ${LZO_INCLUDE_DIR})

set(Z_LIBS ${ZLIB_LIBRARIES})
set(BZ2_LIBS ${BZIP2_LIBRARIES})
set(LZO_LIBS ${LZO_LIBRARIES})

set(CMPRS_INCLUDE ${Z_INCLUDE} ${BZ2_INCLUDE} ${LZO_INCLUDE})
set(CMPRS_LIBS ${Z_LIBS} ${BZ2_LIBS} ${LZO_LIBS})
set(COMPRESS_LIBS xcompress ${CMPRS_LIBS})



