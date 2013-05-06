# $Id$
# Author:  Josh Cherry

# Build AGP file converter app

APP = agpconvert
SRC = agpconvert

LIB  = xalgoseq xobjedit $(OBJREAD_LIBS) taxon1 xalnmgr xobjutil submit \
    ncbi_xdbapi_ftds $(FTDS_LIB) tables xregexp $(PCRE_LIB) $(OBJMGR_LIBS)
LIBS = $(FTDS_LIBS) $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

REQUIRES = objects algo

WATCHERS = xiangcha
