# $Id$

APP = blastinput_unit_test
SRC = blastinput_unit_test blast_scope_src_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

REGEX_LIBS = xregexp $(PCRE_LIB)
ENTREZ_LIBS = entrez2cli entrez2

LIB_ = $(BLAST_FORMATTER_LIBS) $(ENTREZ_LIBS) $(BLAST_LIBS) \
	$(REGEX_LIBS) $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) \
		$(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)
LDFLAGS = $(FAST_LDFLAGS)

REQUIRES = objects

CHECK_CMD = blastinput_unit_test
CHECK_COPY = data
CHECK_AUTHORS = blastsoft
