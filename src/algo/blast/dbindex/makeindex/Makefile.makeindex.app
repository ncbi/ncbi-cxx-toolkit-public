APP = makeindex
SRC = main mkindex_app

LIB = xalgoblastdbindex xnetblastcli xnetblast scoremat seqdb blastdb tables \
      xobjread xobjutil $(OBJMGR_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIBS = $(ORIG_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS)

REQUIRES = objects
