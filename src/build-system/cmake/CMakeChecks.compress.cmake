############################################################################
#
# Compression libraries

#find Zlib using find_package but also honoring --with-z=<DIR> and --without-z
find_external_library(Z INCLUDES zlib.h LIBS z PACKAGE_NAME ZLIB)

#find BZip2 using find_package but also honoring --with-bz2=<DIR> and --without-bz2
include(FindBZip2)
find_external_library(BZ2 INCLUDES bzlib.h LIBS bz2 PACKAGE_NAME BZip2)
#message(FATAL_ERROR "BZ2 INCLUDE: ${BZ2_INCLUDE} LIBS: ${BZ2_LIBS}")

find_external_library(LZO INCLUDES lzo/lzoconf.h LIBS lzo2 HINTS "${NCBI_TOOLS_ROOT}/lzo-2.05/")

set(CMPRS_INCLUDE ${Z_INCLUDE} ${BZ2_INCLUDE} ${LZO_INCLUDE})
set(CMPRS_LIBS ${Z_LIBS} ${BZ2_LIBS} ${LZO_LIBS})
set(COMPRESS_LIBS xcompress ${CMPRS_LIBS})

#message(FATAL_ERROR "INCLUDE: ${CMPRS_INCLUDE} LIBS: ${CMPRS_LIBS}")


