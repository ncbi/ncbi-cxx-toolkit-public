# $Id$

APP = test_seqio
SRC = test_seqio

REQUIRES = Boost.Test.Included

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = test_boost seqset $(SEQ_LIBS) pub medline biblio general xser xutil xncbi

CHECK_CMD =
CHECK_REQUIRES = in-house-resources
CHECK_TIMEOUT = 400

WATCHERS = vasilche gouriano
