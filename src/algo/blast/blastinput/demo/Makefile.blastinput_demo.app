# $Id$

APP = blastinput_demo
SRC = blastinput_demo

CPPFLAGS = $(ORIG_CPPFLAGS)

ENTREZ_LIBS = entrez2cli entrez2

LIB_ = $(BLAST_INPUT_LIBS) ncbi_xloader_blastdb_rmt $(ENTREZ_LIBS) $(BLAST_LIBS) $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

LIBS = $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)
LDFLAGS = $(FAST_LDFLAGS)

REQUIRES = objects -Cygwin

WATCHERS = blastsoft
