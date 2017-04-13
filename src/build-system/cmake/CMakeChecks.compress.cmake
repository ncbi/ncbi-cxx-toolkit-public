############################################################################
#
# Compression libraries

#find Zlib using find_package but also honoring --with-z=<DIR> and --without-z
find_external_library(Z INCLUDES zlib.h LIBS z DO_NOT_UPDATE_MESSAGE)
if (NOT ${Z_FOUND} AND NOT ${Z_DISABLED} AND ${Z} STREQUAL "")
  #try standard package
  include(FindZLIB)
  find_package(ZLIB)
  set(Z_FOUND ZLIB_FOUND)
  if (ZLIB_FOUND)
    set(Z_INCLUDE ${ZLIB_INCLUDE_DIRS})
    set(Z_LIBS ${ZLIB_LIBRARIES})
  endif()
endif()
update_final_message("Z" "Zlib")

#find BZip2 using find_package but also honoring --with-bz2=<DIR> and --without-bz2
find_external_library(BZ2 INCLUDES bzlib.h LIBS bz2 DO_NOT_UPDATE_MESSAGE)
if (NOT ${BZ2_FOUND} AND NOT ${BZ2_DISABLED} AND ${BZ} STREQUAL "")
  #try standard package
  include(FindBZip2)
  find_package(BZip2)
  set(BZ2_FOUND BZIP2_FOUND)
  if (BZIP2_FOUND)
    set(BZ2_INCLUDE ${BZIP2_INCLUDE_DIRS})
    set(BZ2_LIBS ${BZIP2_LIBRARIES})
  endif()
endif()
update_final_message("BZ2" "BZip2")

find_external_library(LZO INCLUDES lzo/lzoconf.h LIBS lzo2 HINTS "${NCBI_TOOLS_ROOT}/lzo-2.05/")

set(CMPRS_INCLUDE ${Z_INCLUDE} ${BZ2_INCLUDE} ${LZO_INCLUDE})
set(CMPRS_LIBS ${Z_LIBS} ${BZ2_LIBS} ${LZO_LIBS})
set(COMPRESS_LIBS xcompress ${CMPRS_LIBS})

