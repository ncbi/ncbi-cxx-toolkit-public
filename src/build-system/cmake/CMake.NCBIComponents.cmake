#############################################################################
# $Id$
#############################################################################

##
## NCBI CMake components description
##
##
## As a result, the following variables should be defined for component XXX
##  NCBI_COMPONENT_XXX_FOUND
##  NCBI_COMPONENT_XXX_INCLUDE
##  NCBI_COMPONENT_XXX_DEFINES
##  NCBI_COMPONENT_XXX_LIBS
##  HAVE_LIBXXX


#############################################################################
set(NCBI_REQUIRE_MT_FOUND YES)

if (NCBI_EXPERIMENTAL_DISABLE_HUNTER)

if (MSVC)
  include(${top_src_dir}/src/build-system/cmake/CMake.NCBIComponentsMSVC.cmake)
elseif (XCODE)
  include(${top_src_dir}/src/build-system/cmake/CMake.NCBIComponentsXCODE.cmake)
else()
  include(${top_src_dir}/src/build-system/cmake/CMake.NCBIComponentsUNIX.cmake)
endif()

else()
  include(${top_src_dir}/src/build-system/cmake/CMake.NCBIComponentsUNIX.cmake)
endif()
