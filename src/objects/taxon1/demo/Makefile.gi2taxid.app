# $Id$
# Makefile for 'gi2taxid' demo app
#

APP = gi2taxid
SRC = gi2taxid

LIB = taxon1 $(SEQ_LIBS) pub medline biblio general xser xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(FLTK_LIBS) $(ORIG_LIBS)
