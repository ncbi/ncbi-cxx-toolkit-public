APP = blast
SRC = blast_app blast_input
LIB = xblastformat xblast composition_adjustment \
      seqdb xnetblastcli xnetblast xalgodustmask \
      ncbi_xloader_blastdb scoremat blastdb xalnmgr \
      xobjsimple xobjutil xobjread creaders connect tables \
      xalgoblastdbindex $(OBJMGR_LIBS)

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)
NCBI_C_LIBS = -lblastapi -lncbitool -lncbiobj -lncbi
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects C-Toolkit
