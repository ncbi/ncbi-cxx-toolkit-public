# $Id$

APP = merge_sub_caches
SRC = merge_sub_caches merge_sub_indices

LIB = asn_cache  seqset $(SEQ_LIBS) pub medline biblio general \
	  bdb xser xconnect $(COMPRESS_LIBS) xregexp \
	  xutil xncbi $(PCRE_LIB)

LIBS = $(BERKELEYDB_LIBS) $(PCRE_LIBS) $(CMPRS_LIBS) \
	   $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS)

WATCHERS = marksc2
