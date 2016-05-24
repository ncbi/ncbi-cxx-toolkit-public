# $Id$

APP = cache_index_copy
SRC = cache_index_copy

LIB  = asn_cache  \
	   seqset $(SEQ_LIBS) pub medline biblio general xser \
	   bdb $(COMPRESS_LIBS) xutil xncbi
LIBS = $(BERKELEYDB_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = marksc2
