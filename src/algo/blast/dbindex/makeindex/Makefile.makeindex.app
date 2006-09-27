APP = makeindex
SRC = main mkindex_app

LIB = xalgoblastdbindex xblast xnetblastcli xnetblast scoremat seqdb blastdb tables \
      xobjread xobjutil 

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIBS = $(OBJMGR_LIBS) $(ORIG_LIBS)

REQUIRES = objmgr objects C-Toolkit
