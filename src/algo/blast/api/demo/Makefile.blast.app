APP = blast
SRC = blast_app seqsrc_readdb blast_format blast_input
LIB = xblast xalnutil xalnmgr ncbi_xloader_blastdb blastdb xobjutil xobjread $(OBJMGR_LIBS)

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  += $(FAST_LDFLAGS)

REQUIRES = objects dbapi
