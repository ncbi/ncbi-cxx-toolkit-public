APP = blast
SRC = blast_app blast_input blast_tabular
LIB = xblast xnetblastcli xnetblast scoremat ncbi_xloader_blastdb \
      xalnutil seqdb blastdb xalnmgr xblastformat xobjutil xobjread connect tables $(OBJMGR_LIBS)

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)
NCBI_C_LIBS = -lblastapi -lncbitool -lncbiobj -lncbi
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

