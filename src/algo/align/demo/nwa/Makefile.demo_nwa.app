#################################
# $Id$
# Author:  Yuri Kapustin (kapustin@ncbi.nlm.nih.gov)
#################################

# Build demo "demo_nwa"
#################################

APP = demo_nwa
SRC = nwa starter

LIB = xalgo xobjmgr id1 seqset seqloc $(SEQ_LIBS) pub medline biblio \
      general xser xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)
