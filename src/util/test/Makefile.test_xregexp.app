# $Id$

APP = test_xregexp
SRC = test_xregexp

LIB = xregexp $(PCRE_LIB) xutil test_boost xncbi
LIBS = $(PCRE_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(BOOST_INCLUDE) $(PCRE_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = Boost.Test.Included

CHECK_CMD = test_xregexp

WATCHERS = ivanov
