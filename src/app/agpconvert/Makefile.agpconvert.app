# $Id$
# Author:  Josh Cherry

# Build AGP file converter app

APP = agpconvert
SRC = agpconvert
LIB = xalgoseq tables xobjread \
      $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

REQUIRES = objects dbapi
