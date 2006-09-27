APP = makeindex
SRC = main mkindex_app

LIB = xalgoblastdbindex xblast xnetblastcli xnetblast scoremat seqdb blastdb tables \
      xobjread xobjutil $(OBJMGR_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIBS = $(ORIG_LIBS) $(NETWORK_LIBS) -lz

REQUIRES = objects C-Toolkit
