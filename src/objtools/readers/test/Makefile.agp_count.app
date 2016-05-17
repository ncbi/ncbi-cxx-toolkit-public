#################################
# $Id$
#################################

APP = agp_count
SRC = agp_count

LIB = $(OBJREAD_LIBS) seqset $(SEQ_LIBS) pub medline biblio general \
      xser xutil xncbi

WATCHERS = sapojnik
