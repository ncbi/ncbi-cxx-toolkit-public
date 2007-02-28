APP = remote_blast
SRC = remote_blast_demo blast_input search_opts queue_poll align_parms
LIB = $(BLAST_FORMATTER_LIBS) $(BLAST_LIBS) $(OBJMGR_LIBS)

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects dbapi -Cygwin
