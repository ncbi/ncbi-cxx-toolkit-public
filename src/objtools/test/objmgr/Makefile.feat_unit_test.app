#################################
# $Id$
#################################

REQUIRES = dbapi FreeTDS Boost.Test.Included

APP = feat_unit_test
SRC = feat_unit_test
LIB = test_boost xobjutil ncbi_xdbapi_ftds $(OBJMGR_LIBS) $(FTDS_LIB)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIBS = $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = feat_unit_test

WATCHERS = vasilche
