APP = remote_blast
SRC = remote_blast_demo search_opts queue_poll align_parms
LIB = xblast xnetblastcli xnetblast seqdb scoremat xobjutil xobjread tables \
      xalnutil xalnmgr blastdb $(OBJMGR_LIBS)

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects dbapi
