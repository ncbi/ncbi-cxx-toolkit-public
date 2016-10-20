############################################################################
#
# Compression libraries
include(FindZLIB)
find_package(ZLIB)
include(FindBZip2)
find_package(BZip2)
FIND_PATH(LZO_INCLUDE_DIR lzo/lzoconf.h
          ${LZO_ROOT}/include/lzo/
          ${NCBI_TOOLS_ROOT}/lzo-2.05/include/
          /usr/include/lzo/
          /usr/local/include/lzo/
          /sw/lib/lzo/
          /sw/local/lib/lzo/
         )
FIND_LIBRARY(LZO_LIBRARIES NAMES lzo2
             PATHS
             ${LZO_ROOT}/lib
             ${NCBI_TOOLS_ROOT}/lzo-2.05/lib64/
             ${NCBI_TOOLS_ROOT}/lzo-2.05/lib/
             /sw/lib
             /sw/local/lib
             /usr/lib
             /usr/local/lib
            )

set(Z_INCLUDE ${ZLIB_INCLUDE_DIRS})
set(Z_LIBS ${ZLIB_LIBRARIES})
set(Z_LIB)
set(BZ2_INCLUDE ${BZIP2_INCLUDE_DIRS})
set(BZ2_LIBS ${BZIP2_LIBRARIES})
set(BZ2_LIB)
set(LZO_INCLUDE ${LZO_INCLUDE_DIR})
set(LZO_LIBS ${LZO_LIBRARIES})


set(CMPRS_INCLUDE ${Z_INCLUDE} ${BZ2_INCLUDE} ${LZO_INCLUDE})
set(CMPRS_LIBS ${Z_LIBS} ${BZ2_LIBS} ${LZO_LIBS})
set(COMPRESS_LIBS xcompress ${CMPRS_LIBS})

