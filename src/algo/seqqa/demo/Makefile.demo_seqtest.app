# $Id$

ASN_DEP = seq

APP = demo_seqtest
SRC = demo_seqtest
LIB = xalgoseqqa xalgoseq xobjutil seqtest xalgognomon entrez2cli entrez2 \
	xalnmgr tables xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS)
LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects

