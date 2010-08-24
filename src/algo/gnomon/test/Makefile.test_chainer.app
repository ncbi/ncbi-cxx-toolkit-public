# $Id$

APP = test_chainer
SRC = test_chainer

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = xalgognomon xalgoseq xalnmgr xobjread xobjutil taxon1 creaders \
       tables xregexp $(PCRE_LIB) xconnect test_boost $(SOBJMGR_LIBS)
LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_COPY = test_chainer.ini
CHECK_CMD =

WATCHERS = chetvern
