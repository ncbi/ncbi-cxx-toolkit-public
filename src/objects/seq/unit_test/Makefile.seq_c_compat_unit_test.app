# $Id$
APP = seq_c_compat_unit_test
SRC = seq_c_compat_unit_test

CPPFLAGS = $(BOOST_INCLUDE) $(NCBI_C_INCLUDE) $(ORIG_CPPFLAGS)

LIB = $(SEQ_LIBS) pub medline biblio general xser xutil test_boost xncbi
LIBS = $(NCBI_C_LIBPATH) -lncbiobj -lncbi $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD =

WATCHERS = ucko

REQUIRES = C-Toolkit
