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
set(NCBI_ALL_COMPONENTS "")
set(NCBI_REQUIRE_MT_FOUND YES)

#############################################################################
# local_lbsm
if(WIN32)
  set(NCBI_COMPONENT_local_lbsm_FOUND NO)
else()
  if (EXISTS ${NCBI_SRC_ROOT}/connect/ncbi_lbsm.c)
#    message("local_lbsm found at ${NCBI_SRC_ROOT}/connect")
    set(NCBI_COMPONENT_local_lbsm_FOUND YES)
    set(HAVE_LOCAL_LBSM 1)
    set(LOCAL_LBSM ncbi_lbsm ncbi_lbsm_ipc ncbi_lbsmd)
  else()
#    message("Component local_lbsm ERROR: not found")
    set(NCBI_COMPONENT_local_lbsm_FOUND NO)
  endif()
endif()

#############################################################################
# LocalPCRE
if (EXISTS ${includedir}/util/regexp)
  set(NCBI_COMPONENT_LocalPCRE_FOUND YES)
  set(NCBI_COMPONENT_LocalPCRE_INCLUDE ${includedir}/util/regexp)
  set(NCBI_COMPONENT_LocalPCRE_NCBILIB regexp)
else()
  set(NCBI_COMPONENT_LocalPCRE_FOUND NO)
endif()

#############################################################################
# LocalZ
if (EXISTS ${includedir}/util/compress/zlib)
  set(NCBI_COMPONENT_LocalZ_FOUND YES)
  set(NCBI_COMPONENT_LocalZ_INCLUDE ${includedir}/util/compress/zlib)
  set(NCBI_COMPONENT_LocalZ_NCBILIB z)
else()
  set(NCBI_COMPONENT_LocalZ_FOUND NO)
endif()

#############################################################################
# LocalBZ2
if (EXISTS ${includedir}/util/compress/bzip2)
  set(NCBI_COMPONENT_LocalBZ2_FOUND YES)
  set(NCBI_COMPONENT_LocalBZ2_INCLUDE ${includedir}/util/compress/bzip2)
  set(NCBI_COMPONENT_LocalBZ2_NCBILIB bz2)
else()
  set(NCBI_COMPONENT_LocalBZ2_FOUND NO)
endif()

#############################################################################
#LocalLMDB
if (EXISTS ${includedir}/util/lmdb)
  set(NCBI_COMPONENT_LocalLMDB_FOUND YES)
  set(NCBI_COMPONENT_LocalLMDB_INCLUDE ${includedir}/util/lmdb)
  set(NCBI_COMPONENT_LocalLMDB_NCBILIB lmdb)
else()
  set(NCBI_COMPONENT_LocalLMDB_FOUND NO)
endif()

#############################################################################
# FreeTDS
set(FTDS64_INCLUDE  ${includedir}/dbapi/driver/ftds64  ${includedir}/dbapi/driver/ftds64/freetds)
set(FTDS95_INCLUDE  ${includedir}/dbapi/driver/ftds95  ${includedir}/dbapi/driver/ftds95/freetds)
set(FTDS100_INCLUDE ${includedir}/dbapi/driver/ftds100 ${includedir}/dbapi/driver/ftds100/freetds)

set(NCBI_COMPONENT_FreeTDS_FOUND   YES)
set(NCBI_COMPONENT_FreeTDS_INCLUDE ${FTDS95_INCLUDE})
#set(NCBI_COMPONENT_FreeTDS_LIBS    ct_ftds95)

#############################################################################
set(NCBI_COMPONENT_Boost.Test.Included_NCBILIB test_boost)
set(NCBI_COMPONENT_SQLITE3_NCBILIB sqlitewrapp)
set(NCBI_COMPONENT_Sybase_NCBILIB  ncbi_xdbapi_ctlib)
set(NCBI_COMPONENT_ODBC_NCBILIB    ncbi_xdbapi_odbc)
set(NCBI_COMPONENT_FreeTDS_NCBILIB ct_ftds95 ncbi_xdbapi_ftds)

#############################################################################
if (NCBI_EXPERIMENTAL_DISABLE_HUNTER)

if (MSVC)
  include(${NCBI_TREE_CMAKECFG}//CMake.NCBIComponentsMSVC.cmake)
elseif (XCODE)
  include(${NCBI_TREE_CMAKECFG}//CMake.NCBIComponentsXCODE.cmake)
else()
  include(${NCBI_TREE_CMAKECFG}//CMake.NCBIComponentsUNIX.cmake)
endif()

else()
  include(${top_src_dir}/src/build-system/cmake/CMake.NCBIComponentsUNIX.cmake)
endif()

#############################################################################
# FreeTDS
set(FTDS64_INCLUDE  ${includedir}/dbapi/driver/ftds64  ${includedir}/dbapi/driver/ftds64/freetds)
set(FTDS95_INCLUDE  ${includedir}/dbapi/driver/ftds95  ${includedir}/dbapi/driver/ftds95/freetds)
set(FTDS100_INCLUDE ${includedir}/dbapi/driver/ftds100 ${includedir}/dbapi/driver/ftds100/freetds)
