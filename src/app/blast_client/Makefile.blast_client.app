# $Id$

APP = blast_client
SRC = blast_client queue_poll search_opts align_parms
LIB = xblastformat xblast xalgoblastdbindex composition_adjustment seqdb \
      xnetblastcli xnetblast xalgodustmask blastxml blastdb \
      scoremat xalnmgr xobjsimple xobjutil xobjread creaders tables xhtml \
      $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
LDFLAGS = $(FAST_LDFLAGS)

REQUIRES = objects dbapi
