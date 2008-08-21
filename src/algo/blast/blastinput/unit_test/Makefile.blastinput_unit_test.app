# $Id$

APP = blastinput_unit_test
SRC = blastinput_unit_test blast_scope_src_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

ENTREZ_LIBS = entrez2cli entrez2

LIB_ = $(ENTREZ_LIBS) $(BLAST_INPUT_LIBS) $(BLAST_LIBS) $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

LIBS = $(BOOST_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) \
		$(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)
LDFLAGS = $(FAST_LDFLAGS)

REQUIRES = objects

CHECK_CMD = blastinput_unit_test
CHECK_COPY = data
CHECK_AUTHORS = blastsoft
