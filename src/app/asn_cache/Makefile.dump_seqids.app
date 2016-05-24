# $Id$

APP = dump_asn_cache_index
SRC = dump_seqids

LIB = asn_cache  seqset $(SEQ_LIBS) pub medline biblio general \
	  bdb xser xconnect xcompress \
	  $(COMPRESS_LIBS) xutil xncbi
LIBS = $(BERKELEYDB_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = marksc2
