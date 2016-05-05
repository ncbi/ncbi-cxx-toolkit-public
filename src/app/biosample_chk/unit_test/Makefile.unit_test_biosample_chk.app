# $Id$

APP = unit_test_biosample_chk
SRC = unit_test_biosample_chk

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = xvalidate xunittestutil $(OBJEDIT_LIBS) $(XFORMAT_LIBS) xalnmgr xobjutil \
       valerr tables xregexp $(PCRE_LIB) test_boost $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD =
CHECK_COPY = 
CHECK_TIMEOUT = 3000

WATCHERS = bollin
