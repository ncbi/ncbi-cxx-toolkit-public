#

APP = xcompareannotsdemo
SRC = xcompareannotsdemo

CPPFLAGS = $(ORIG_CPPFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIB = \
      xalnmgr $(BLAST_LIBS) \
      xalgoalignutil \
      ncbi_xloader_lds lds bdb xobjread pub medline biblio creaders\
      seqset $(SEQ_LIBS) \
      xobjmgr xobjutil $(OBJMGR_LIBS) \
      general xser xncbi xutil \
      xalgoseq
      
LIBS = $(BERKELEYDB_LIBS)  $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
