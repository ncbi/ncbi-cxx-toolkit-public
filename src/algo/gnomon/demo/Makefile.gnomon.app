# $Id$
# Makefile for 'gnomon' app

SRC = gnomon
APP = gnomon

LIB = xalgognomon $(OBJMGR_LIBS)
LIBS = $(ORIG_LIBS) $(NETWORK_LIBS) $(DLL_LIBS)
