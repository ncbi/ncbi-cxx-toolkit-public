# $Id$

APP = blastinput_unit_test
SRC = blastinput_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

BLAST_FORMATTER_LIBS = xblastformat xalnmgr ncbi_xloader_blastdb xcgi xhtml
LIB = blastinput $(BLAST_FORMATTER_LIBS) $(BLAST_LIBS) $(OBJMGR_LIBS)
LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) $(NETWORK_LIBS) \
		$(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)
LDFLAGS = $(FAST_LDFLAGS)

REQUIRES = objects

CHECK_CMD =
CHECK_COPY = data
