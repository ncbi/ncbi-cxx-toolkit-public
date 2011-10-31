#################################
# $Id$
#################################

REQUIRES = objects

APP = agp_val_test
SRC = agp_val_test

LIB = xobjread seqset $(SEQ_LIBS) pub medline biblio general \
      xser xutil xncbi

WATCHERS = sapojnik
