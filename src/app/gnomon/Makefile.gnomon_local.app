# $Id$
# Makefile for 'gnomon_local' app

APP = gnomon_local
SRC = gnomon_local
LIB = xalgognomon xncbi

# These are necessary to avoid build errors in some configurations
# (notably 32-bit SPARC WorkShop Release).
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)
