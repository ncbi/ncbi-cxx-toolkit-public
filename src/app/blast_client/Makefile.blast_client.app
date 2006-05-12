# $Id$

APP = blast_client
SRC = blast_client queue_poll search_opts align_parms
LIB = xblastformat xblast composition_adjustment seqdb \
      xnetblastcli xnetblast blastdb \
      scoremat xalnmgr xobjsimple xobjutil xobjread tables xhtml $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
LDFLAGS = $(FAST_LDFLAGS)

REQUIRES = objects dbapi
