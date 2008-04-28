# $Id$

APP = blast_unit_test
SRC = test_objmgr blast_test_util bl2seq_unit_test gencode_singleton_unit_test \
	blastoptions_unit_test blastfilter_unit_test uniform_search_unit_test \
	remote_blast_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB_ = $(BLAST_INPUT_LIBS) $(BLAST_LIBS) xobjsimple $(OBJMGR_LIBS) xalgowinmask
LIB = $(LIB_:%=%$(STATIC))

LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) $(NETWORK_LIBS) \
		$(PCRE_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)
LDFLAGS = $(FAST_LDFLAGS)

REQUIRES = objects

CHECK_CMD = blast_unit_test
CHECK_AUTHORS = blastsoft

