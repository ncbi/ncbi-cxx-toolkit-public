# $Id$

APP = unit_test_parse_text
SRC = unit_test_parse_text

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = $(OBJEDIT_LIBS) xunittestutil $(XFORMAT_LIBS) xobjutil \
       $(PCRE_LIB) test_boost $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD = unit_test_parse_text
CHECK_TIMEOUT = 3000

WATCHERS = asztalos
