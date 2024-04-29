#################################
# $Id$
#################################

REQUIRES = MT Boost.Test.Included

APP = test_load_lock
SRC = test_load_lock

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = test_boost $(OBJMGR_LIBS) $(FTDS_LIB)

LIBS = $(GENBANK_THIRD_PARTY_LIBS) $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_load_lock

WATCHERS = vasilche
