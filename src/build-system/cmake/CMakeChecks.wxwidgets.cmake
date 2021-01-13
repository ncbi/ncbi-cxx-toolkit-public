# wxWidgets
set(_foo_CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}")
if (WIN32)
    set(CMAKE_PREFIX_PATH ${WXWIDGETS_ROOT})
else()
    set(CMAKE_PREFIX_PATH "${NCBI_TOOLS_ROOT}/wxWidgets-3.1.3-ncbi1/${STD_BUILD_TYPE}/bin")
endif()

set(wxWidgets_USE_UNICODE ON)
set(wxWidgets_USE_SHAREED_LIBS ${BUILD_SHARED_LIBS})

if ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
    set(wxWidgets_CONFIG_OPTIONS "--debug=yes")
    set(wxWidgets_USE_DEBUG ON)
else()
    set(wxWidgets_USE_DEBUG OFF)
endif()

find_package(GTK2)
if (GTK2_FOUND)
  set(WXWIDGETS_INCLUDE
    ${NCBI_TOOLS_ROOT}/wxWidgets-3.1.3-ncbi1/${NCBI_BUILD_TYPE}/lib/wx/include/gtk2-ansi-3.1
    ${NCBI_TOOLS_ROOT}/wxWidgets-3.1.3-ncbi1/include/wx-3.1
    ${GTK2_INCLUDE_DIRS}
  )
  set(_wxp ${NCBI_TOOLS_ROOT}/wxWidgets-3.1.3-ncbi1/${NCBI_BUILD_TYPE}/lib/lib)
  set(_wxs -3.1.so)
  set(WXWIDGETS_LIBS
    ${_wxp}wx_gtk2_gl${_wxs}
    ${_wxp}wx_gtk2_richtext${_wxs}
    ${_wxp}wx_gtk2_aui${_wxs}
    ${_wxp}wx_gtk2_propgrid${_wxs}
    ${_wxp}wx_gtk2_xrc${_wxs}
    ${_wxp}wx_gtk2_html${_wxs}
    ${_wxp}wx_gtk2_qa${_wxs}
    ${_wxp}wx_gtk2_adv${_wxs}
    ${_wxp}wx_gtk2_core${_wxs}
    ${_wxp}wx_base_xml${_wxs}
    ${_wxp}wx_base_net${_wxs}
    ${_wxp}wx_base${_wxs}
    ${GTK2_LIBRARIES}
  )
  set(WXWIDGETS_FOUND YES)
endif()

set(CMAKE_PREFIX_PATH "${_foo_CMAKE_PREFIX_PATH}")


#FIXME: this is broken
#include( ${wxWidgets_USE_FILE} )

## message(STATUS "wxWidgets: CONFIG_OPTIONS   = ${wxWidgets_CONFIG_OPTIONS}")
## message(STATUS "wxWidgets: DEBUG            = ${wxWidgets_USE_DEBUG}")
## message(STATUS "wxWidgets: SHARED_LIBS      = ${wxWidgets_USE_SHAREED_LIBS}")
## message(STATUS "wxWidgets: CXX_FLAGS        = ${wxWidgets_CXX_FLAGS}")
## message(STATUS "wxWidgets: DEFINITIONS      = ${wxWidgets_DEFINITIONS}")
## message(STATUS "wxWidgets: LIBRARIES        = ${wxWidgets_LIBRARIES}")
## message(STATUS "wxWidgets: USE_FILE         = ${wxWidgets_USE_FILE}")


