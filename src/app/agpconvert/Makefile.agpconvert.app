# $Id$
# Author:  Josh Cherry

# Build AGP file converter app

APP = agpconvert
SRC = agpconvert
LIB = xalgoseq xalnmgr xobjutil tables xobjread xregexp $(PCRE_LIB) \
      $(OBJMGR_LIBS)
LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

REQUIRES = objects dbapi
