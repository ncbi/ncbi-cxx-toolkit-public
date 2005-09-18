# $Id$
# Makefile for 'localfinder' app

SRC = local_finder
APP = localfinder

LIB = xalgognomon xobjutil $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)

#REQUIRES = algo/gnomon
