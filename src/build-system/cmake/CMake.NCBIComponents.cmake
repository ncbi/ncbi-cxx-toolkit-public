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
if (NOT WIN32)
  return()
endif()

#############################################################################
# common settings
set(NCBI_ThirdPartyBasePath //snowman/win-coremake/Lib/ThirdParty)
set(NCBI_ThirdPartyCompiler vs2015.64)

set(NCBI_ThirdParty_Boost ${NCBI_ThirdPartyBasePath}/boost/${NCBI_ThirdPartyCompiler}/1.61.0)
set(NCBI_ThirdParty_PCRE  ${NCBI_ThirdPartyBasePath}/pcre/${NCBI_ThirdPartyCompiler}/7.9)
set(NCBI_ThirdParty_Z     ${NCBI_ThirdPartyBasePath}/z/${NCBI_ThirdPartyCompiler}/1.2.8)
set(NCBI_ThirdParty_JPEG  ${NCBI_ThirdPartyBasePath}/jpeg/${NCBI_ThirdPartyCompiler}/6b)
set(NCBI_ThirdParty_PNG   ${NCBI_ThirdPartyBasePath}/png/${NCBI_ThirdPartyCompiler}/1.2.7)
set(NCBI_ThirdParty_GIF   ${NCBI_ThirdPartyBasePath}/gif/${NCBI_ThirdPartyCompiler}/4.1.3)
set(NCBI_ThirdParty_TIFF  ${NCBI_ThirdPartyBasePath}/tiff/${NCBI_ThirdPartyCompiler}/3.6.1)


#############################################################################
macro(NCBI_define_component _name _lib)
  set(_root ${NCBI_ThirdParty_${_name}})
  if (EXISTS ${_root}/include)
    set(_found YES)
  else()
    message("Component ${_name} ERROR: ${_root}/include not found")
    set(_found NO)
  endif()
  if (_found)
    set(_libtype lib_static)
    foreach(_cfg ${CMAKE_CONFIGURATION_TYPES})
      if(NOT EXISTS ${_root}/${_libtype}/${_cfg}/${_lib})
        message("Component ${_name} ERROR: ${_root}/${_libtype}/${_cfg}/${_lib} not found")
        set(_found NO)
      endif()
    endforeach()
  endif()
  if (_found)
    set(NCBI_COMPONENT_${_name}_FOUND YES)
    set(NCBI_COMPONENT_${_name}_INCLUDE ${_root}/include)
    set(NCBI_COMPONENT_${_name}_LIBS ${_root}/${_libtype}/\$\(Configuration\)/${_lib})
    string(TOUPPER ${_name} _upname)
    set(HAVE_LIB${_upname} 1)
  else()
    set(NCBI_COMPONENT_${_name}_FOUND NO)
  endif()
endmacro()

#############################################################################
# Boost.Test.Included
if (EXISTS ${NCBI_ThirdParty_Boost}/include)
  set(NCBI_COMPONENT_Boost.Test.Included_FOUND YES)
  set(NCBI_COMPONENT_Boost.Test.Included_INCLUDE ${NCBI_ThirdParty_Boost}/include)
  set(NCBI_COMPONENT_Boost.Test.Included_DEFINES BOOST_TEST_NO_LIB)
else()
  message("Component Boost.Test.Included ERROR: ${NCBI_ThirdParty_Boost}/include not found")
  set(NCBI_COMPONENT_Boost.Test.Included_FOUND NO)
endif()

#############################################################################
#LocalPCRE

set(NCBI_COMPONENT_LocalPCRE_FOUND YES)
set(NCBI_COMPONENT_LocalPCRE_INCLUDE ${includedir}/util/regexp)
set(NCBI_COMPONENT_LocalPCRE_LIBS regexp)

#############################################################################
# PCRE
NCBI_define_component(PCRE libpcre.lib)
if(NOT NCBI_COMPONENT_PCRE_FOUND)
  set(NCBI_COMPONENT_PCRE_FOUND YES)
  set(NCBI_COMPONENT_PCRE_INCLUDE ${NCBI_COMPONENT_LocalPCRE_INCLUDE})
  set(NCBI_COMPONENT_PCRE_LIBS ${NCBI_COMPONENT_LocalPCRE_LIBS})
endif()

#############################################################################
# Z
NCBI_define_component(Z libz.lib)

#############################################################################
# JPEG
NCBI_define_component(JPEG libjpeg.lib)

#############################################################################
# PNG
NCBI_define_component(PNG libpng.lib)

#############################################################################
# GIF
NCBI_define_component(GIF libgif.lib)

#############################################################################
# TIFF
NCBI_define_component(TIFF libtiff.lib)


