#################################
# $Id$
#################################

REQUIRES = objects

APP = agp_renumber
SRC = agp_renumber

LIB = $(OBJREAD_LIBS) seqset $(SEQ_LIBS) pub medline biblio general \
      xser xutil xncbi

WATCHERS = sapojnik
