# $Id$

APP = blast_client
SRC = blast_client queue_poll search_opts align_parms
LIB = xblastformat blastxml xalnmgr xhtml xcgi $(BLAST_LIBS) $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
LDFLAGS = $(FAST_LDFLAGS)

REQUIRES = objects dbapi -Cygwin
