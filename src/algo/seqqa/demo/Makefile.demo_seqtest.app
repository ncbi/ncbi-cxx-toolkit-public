# $Id$

ASN_DEP = seq

APP = demo_seqtest
SRC = demo_seqtest
LIB = xalgoseqqa xalgoseq xobjutil seqtest xalgognomon entrez2cli entrez2 \
	xalnmgr tables $(OBJMGR_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS)
LIBS = $(CMPRS_LIBS) $(ORIG_LIBS)

REQUIRES = objects

