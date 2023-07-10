# $Id$
# Makefile for 'exon_selector' app

SRC = exon_selector
APP = exon_selector

LIB = $(OBJMGR_LIBS)

LIBS = $(ORIG_LIBS)

# These are necessary to avoid build errors in some configurations
# (notably 32-bit SPARC WorkShop Release).
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)
