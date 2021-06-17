############################################################################
#
# Various image-format libraries
set(_foo_CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})

if (WIN32)
    set(CMAKE_PREFIX_PATH ${JPEG_ROOT} ${PNG_ROOT} ${TIFF_ROOT})
endif()

find_package(JPEG)
find_package(PNG)
find_package(TIFF)
find_package(GIF)
find_package(Freetype)
if (FREETYPE_FOUND)
    find_package(FTGL)
endif()

#
# For backward compatibility

set(IMAGE_INCLUDE_DIR
    ${JPEG_INCLUDE_DIR}
    ${PNG_INCLUDE_DIR}
    ${TIFF_INCLUDE_DIR}
    ${GIF_INCLUDE_DIR})
set(IMAGE_LIBS
    ${JPEG_LIBRARIES}
    ${PNG_LIBRARIES}
    ${TIFF_LIBRARIES}
    ${GIF_LIBRARIES})


# FreeType, FTGL
set(FREETYPE_INCLUDE ${FREETYPE_INCLUDE_DIRS})
set(FREETYPE_LIBS    ${FREETYPE_LIBRARIES})
set(FTGL_INCLUDE     ${FTGL_INCLUDE_DIR} ${FREETYPE_INCLUDE_DIRS})
set(FTGL_LIBS        ${FTGL_LIBRARIES} ${FREETYPE_LIBRARIES})

set(CMAKE_PREFIX_PATH ${_foo_CMAKE_PREFIX_PATH})

