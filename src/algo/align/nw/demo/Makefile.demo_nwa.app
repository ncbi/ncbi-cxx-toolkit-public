#################################
# $Id$
# Author:  Yuri Kapustin (kapustin@ncbi.nlm.nih.gov)
#################################

# Build demo "demo_nwa"
#################################

APP = demo_nwa
SRC = nwa starter


#LIB = xalgo xobjutil xobjmgr id1 seqset $(SEQ_LIBS) \
	  seq seqres seqblock seqfeat \
	  pub medline biblio \
      general dbapi_driver xser xutil xconnect xncbi

#LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)


LIB = xalgo xobjmgr seqloc $(SEQ_LIBS) pub medline biblio \
      general xser xutil seqfeat xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)
