#############################################################################
# $Id$
#############################################################################

cmake_minimum_required(VERSION 3.15)
if ("${PROJECT_NAME}" STREQUAL "")
    project(ncbi_cpp)
endif()
include(${CMAKE_CURRENT_LIST_DIR}/build-system/cmake/CMakeLists.top_builddir.txt)
