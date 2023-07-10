# $Id$
# Makefile for 'compact_sam' app

SRC = compact_sam
APP = compact_sam

LIB = xalgoalignsplign xalgoalignutil $(OBJMGR_LIBS)

LIBS = $(ORIG_LIBS)

# These are necessary to avoid build errors in some configurations
# (notably 32-bit SPARC WorkShop Release).
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)
