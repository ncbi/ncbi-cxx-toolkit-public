# $Id$

SRC = regexplocdemo
APP = regexplocdemo

CPPFLAGS = $(ORIG_CPPFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIB = xalgoseq xobjmgr id1 seqset $(SEQ_LIBS) pub medline biblio \
      general dbapi_driver xser xconnect xutil xncbi xregexp regexp \

LIBS = $(ORIG_LIBS) $(DL_LIBS) $(NETWORK_LIBS)
