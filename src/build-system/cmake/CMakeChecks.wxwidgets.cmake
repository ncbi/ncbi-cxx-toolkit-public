# wxWidgets
set(foo "${CMAKE_PREFIX_PATH}")
#set(CMAKE_PREFIX_PATH "${NCBI_TOOLS_ROOT}/wxwidgets/${CMAKE_BUILD_TYPE}/bin")
set(CMAKE_PREFIX_PATH "${NCBI_TOOLS_ROOT}/wxWidgets-3.0.1-ncbi1/${CMAKE_BUILD_TYPE}/bin")

set(wxWidgets_USE_UNICODE ON)
set(wxWidgets_USE_SHAREED_LIBS ${BUILD_SHARED_LIBS})

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(wxWidgets_CONFIG_OPTIONS "--debug=yes")
    set(wxWidgets_USE_DEBUG ON)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    set(wxWidgets_USE_DEBUG OFF)
endif()

if (EXISTS ${NCBI_TOOLS_ROOT}/wxWidgets-3.0.1-ncbi1)

  set(WXWIDGETS_INCLUDE ${NCBI_TOOLS_ROOT}/wxWidgets-3.0.1-ncbi1/include)
  set(WXWIDGETS_LIBS "${NCBI_TOOLS_ROOT}/wxWidgets-3.0.1-ncbi1/${CMAKE_BUILD_TYPE}MT64/libs")
  set(WXWIDGETS_STATIC_LIBS "${NCBI_TOOLS_ROOT}/wxWidgets-3.0.1-ncbi1/${CMAKE_BUILD_TYPE}MT64/libs")
  set(WXWIDGETS_GL_LIBS -lwx_gtk2_gl-2.9 -lwx_base-2.9)

else (EXISTS ${NCBI_TOOLS_ROOT}/wxWidgets-3.0.1-ncbi1)

  include(FindwxWidgets)
  FIND_PACKAGE(wxWidgets
    COMPONENTS gl core base )
  set(WXWIDGETS_INCLUDE "${wxWidgets_INCLUDE_DIRS}")
  set(WXWIDGETS_LIBS "${wxWidgets_LIBRARIES}")
  set(WXWIDGETS_STATIC_LIBS ${WXWIDGETS_LIBS})
  set(WXWIDGETS_GL_LIBS -lwx_gtk2_gl-2.9 -lwx_base-2.9)

endif (EXISTS ${NCBI_TOOLS_ROOT}/wxWidgets-3.0.1-ncbi1)

set(CMAKE_PREFIX_PATH "${foo}")
set(foo)


#FIXME: this is broken
#include( ${wxWidgets_USE_FILE} )

## message(STATUS "wxWidgets: CONFIG_OPTIONS   = ${wxWidgets_CONFIG_OPTIONS}")
## message(STATUS "wxWidgets: DEBUG            = ${wxWidgets_USE_DEBUG}")
## message(STATUS "wxWidgets: SHARED_LIBS      = ${wxWidgets_USE_SHAREED_LIBS}")
## message(STATUS "wxWidgets: CXX_FLAGS        = ${wxWidgets_CXX_FLAGS}")
## message(STATUS "wxWidgets: DEFINITIONS      = ${wxWidgets_DEFINITIONS}")
## message(STATUS "wxWidgets: LIBRARIES        = ${wxWidgets_LIBRARIES}")
## message(STATUS "wxWidgets: USE_FILE         = ${wxWidgets_USE_FILE}")


