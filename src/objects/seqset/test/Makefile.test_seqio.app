# $Id$

APP = test_seqio
SRC = test_seqio

REQUIRES = Boost.Test.Included

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = test_boost seqset $(SEQ_LIBS) pub medline biblio general xser xutil xncbi

CHECK_CMD =
CHECK_COPY = test_data

WATCHERS = vasilche
