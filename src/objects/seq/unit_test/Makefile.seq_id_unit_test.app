# $Id$
APP = seq_id_unit_test
SRC = seq_id_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

# Kludge: disable optimization to work around really slow compilation
# (longer than half an hour!) when using Apple's version of GCC 3.3.
CXXFLAGS = $(ORIG_CXXFLAGS) -O0

LIB = $(SEQ_LIBS) pub medline biblio general xser xutil xncbi
LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) $(ORIG_LIBS)

CHECK_CMD =
