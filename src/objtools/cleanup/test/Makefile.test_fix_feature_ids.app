# $Id$

APP = test_fix_feature_ids
SRC = test_fix_feature_ids

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE) $(PCRE_INCLUDE)
LDFLAGS = $(FAST_LDFLAGS)
LIB  = xcleanup $(OBJEDIT_LIBS) xobjutil valid pubmed xconnect \
       xregexp $(PCRE_LIB) test_boost $(SOBJMGR_LIBS)
LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD = test_fix_feature_ids
CHECK_COPY = test_cases

WATCHERS = kachalos
