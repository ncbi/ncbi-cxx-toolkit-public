# $Id$

APP = bdbloader_unit_test
SRC = bdbloader_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE) -I../

LIB_ = $(BLAST_INPUT_LIBS) ncbi_xloader_blastdb_rmt $(BLAST_LIBS) $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) $(NETWORK_LIBS) \
		$(PCRE_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)
LDFLAGS = $(FAST_LDFLAGS)

REQUIRES = objects

CHECK_CMD = bdbloader_unit_test
CHECK_COPY = data
CHECK_AUTHORS = blastsoft
CHECK_TIMEOUT = 600

