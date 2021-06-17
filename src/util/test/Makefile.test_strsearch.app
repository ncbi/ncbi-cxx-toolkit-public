#################################
# $Id$
#################################

# Build test application "test_strsearch"
#################################
APP = test_strsearch
SRC = test_strsearch
LIB = xutil test_boost xncbi

CPPFLAGS = $(BOOST_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = Boost.Test.Included

CHECK_CMD = test_strsearch

WATCHERS = ivanov
