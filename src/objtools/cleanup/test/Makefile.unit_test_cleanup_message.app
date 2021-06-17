# $Id$

APP = unit_test_cleanup_message
SRC = unit_test_cleanup_message

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xcleanup xlogging $(OBJEDIT_LIBS) xunittestutil xobjutil valid xconnect \
      xregexp $(PCRE_LIB) $(COMPRESS_LIBS) test_boost $(SOBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD =
#CHECK_COPY = test_cases
#CHECK_TIMEOUT = 1200

WATCHERS = foleyjp
