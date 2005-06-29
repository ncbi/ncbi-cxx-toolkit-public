# $Id$
APP = seq_id_unit_test
SRC = seq_id_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = $(SEQ_LIBS) pub medline biblio general xser xutil xncbi
LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) $(ORIG_LIBS)

CHECK_CMD =
