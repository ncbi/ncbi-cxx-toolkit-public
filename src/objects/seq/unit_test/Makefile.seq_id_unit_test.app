# $Id$
APP = seq_id_unit_test
SRC = seq_id_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

PRE_LIBS = $(BOOST_TEST_LIBS)
LIB = $(SEQ_LIBS) pub medline biblio general xser xutil test_boost xncbi

CHECK_CMD =
