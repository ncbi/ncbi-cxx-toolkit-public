#################################
# $Id$
# Author:  Yuri Kapustin (kapustin@ncbi.nlm.nih.gov)
#################################

# Build demo "demo_nwa"
#################################

APP = demo_nwa
SRC = nwa starter

LIB = xalgoalign tables $(OBJMGR_LIBS:dbapi_driver=dbapi_driver-static)

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)
