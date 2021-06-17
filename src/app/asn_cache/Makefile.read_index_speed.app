# $Id$

APP = read_index_speed
SRC = read_index_speed

LIB = asn_cache  seqset $(SEQ_LIBS) pub medline biblio general \
	  bdb xser xconnect \
	  $(COMPRESS_LIBS) xutil xncbi
LIBS = $(BERKELEYDB_LIBS) $(CMPRS_LIBS) $(DL_LIBS) \
    $(ORIG_LIBS)

WATCHERS = marksc2
