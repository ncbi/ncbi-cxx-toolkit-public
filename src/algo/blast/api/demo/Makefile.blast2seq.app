APP = blast2seq
SRC = blast2seq blast_input
LIB = xblast xnetblastcli xnetblast scoremat xobjutil xobjread tables \
      $(OBJMGR_LIBS)

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIBS = $(ORIG_LIBS) $(NETWORK_LIBS) $(DL_LIBS)

REQUIRES = objects dbapi
