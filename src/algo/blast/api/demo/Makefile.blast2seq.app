APP = blast2seq
SRC = blast2seq blast_input
LIB = xblast xnetblastcli xnetblast seqdb blastdb scoremat xobjutil xobjread \
      tables $(GENBANK_LDEP)

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects dbapi
