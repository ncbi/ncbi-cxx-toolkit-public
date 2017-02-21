# $Id$

APP = test_ncbi_url
SRC = test_ncbi_url
LIB = test_boost xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

REQUIRES = Boost.Test.Included

CHECK_CMD  =

WATCHERS = grichenk
