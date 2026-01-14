# $Id$

APP = unit_test_gbqual_cleanup
SRC = unit_test_gbqual_cleanup

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xcleanup $(OBJEDIT_LIBS) xobjutil valid xconnect \
      xregexp $(PCRE_LIB) $(COMPRESS_LIBS) test_boost $(SOBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD =
CHECK_TIMEOUT = 1200

WATCHERS = stakhovv kans foleyjp
