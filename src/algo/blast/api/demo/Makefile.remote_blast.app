APP = remote_blast
SRC = remote_blast_demo blast_input search_opts queue_poll align_parms
LIB = xblastformat xblast composition_adjustment blastxml \
      seqdb xnetblastcli xnetblast xalgodustmask scoremat \
      xalnmgr blastdb xobjsimple xobjutil xobjread creaders tables xhtml \
      xalgoblastdbindex $(OBJMGR_LIBS)

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects dbapi -Cygwin
