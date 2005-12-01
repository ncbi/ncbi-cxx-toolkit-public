APP = remote_blast
SRC = remote_blast_demo search_opts queue_poll align_parms
LIB = xblastformat xblast composition_adjustment \
      seqdb xnetblastcli xnetblast scoremat \
      xalnmgr blastdb xobjsimple xobjutil xobjread tables $(OBJMGR_LIBS)

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects dbapi
