# $Id$

APP = update_cache
SRC = update_cache

LIB = asn_cache  seqset $(SEQ_LIBS) pub medline biblio general \
	  bdb xser xconnect xcompress \
	  $(COMPRESS_LIBS) xutil xncbi xregexp $(PCRE_LIB)

LIBS = $(BERKELEYDB_LIBS) $(CMPRS_LIBS) $(PCRE_LIBS) $(DL_LIBS) \
	   $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS)

WATCHERS = marksc2
