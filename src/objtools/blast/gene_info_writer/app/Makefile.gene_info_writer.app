# $Id$

APP = gene_info_writer
SRC = gene_info_writer_app
LIB_ = gene_info_writer gene_info seqdb xobjutil blastdb $(SOBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC)) $(LMDB_LIB)

CFLAGS    = $(FAST_CFLAGS)
CXXFLAGS  = $(FAST_CXXFLAGS)
LDFLAGS   = $(FAST_LDFLAGS)

LIBS = $(BLAST_THIRD_PARTY_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) \
       $(ORIG_LIBS)

WATCHERS = camacho
