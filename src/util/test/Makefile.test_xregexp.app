# $Id$

APP = test_xregexp
SRC = test_xregexp

LIB = xregexp $(PCRE_LIB) xutil test_boost xncbi
LIBS = $(PCRE_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(PCRE_INCLUDE) $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

REQUIRES = Boost.Test.Included

CHECK_CMD = test_xregexp

WATCHERS = ivanov
