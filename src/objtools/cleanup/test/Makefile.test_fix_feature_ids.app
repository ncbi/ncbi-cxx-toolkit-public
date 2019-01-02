# $Id$

APP = test_fix_feature_ids
SRC = test_fix_feature_ids

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xcleanup seqset $(SEQ_LIBS) pub medline biblio general xser xutil test_boost $(SOBJMGR_LIBS) xncbi

LIBS = $(DL_LIBS) $(ORIG_LIBS)
LDFLAGS = $(FAST_LDFLAGS)

REQUIRES = Boost.Test.Included

CHECK_CMD = test_fix_feature_ids
CHECK_COPY = test_cases

WATCHERS = filippov
