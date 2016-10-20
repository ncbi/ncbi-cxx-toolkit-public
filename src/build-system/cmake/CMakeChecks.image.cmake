############################################################################
#
# Various image-format libraries
include(FindJPEG)
find_package(JPEG)

include(FindPNG)
find_package(PNG)

include(FindTIFF)
find_package(TIFF)

include(FindGIF)
find_package(GIF)

set(IMAGE_INCLUDE_DIR ${JPEG_INCLUDE_DIR} ${PNG_INCLUDE_DIR} ${TIFF_INCLUDE_DIR} ${GIF_INCLUDE_DIR})
set(IMAGE_LIBS ${JPEG_LIBRARIES} ${PNG_LIBRARIES} ${TIFF_LIBRARIES} ${GIF_LIBRARIES})


# FreeType, FTGL
set(FREETYPE_INCLUDE /usr/include/freetype2)
set(FREETYPE_LIBS    -lfreetype)
set(FTGL_INCLUDE     /usr/include/freetype2 
                     ${NCBI_TOOLS_ROOT}/ftgl-2.1.3-rc5/include)
set(FTGL_LIBS        -L${NCBI_TOOLS_ROOT}/ftgl-2.1.3-rc5/GCC401-Debug64/lib -Wl,-rpath,/opt/ncbi/64/ftgl-2.1.3-rc5/GCC401-Debug64/lib:${NCBI_TOOLS_ROOT}/ftgl-2.1.3-rc5/GCC401-Debug64/lib -lftgl -lfreetype)

