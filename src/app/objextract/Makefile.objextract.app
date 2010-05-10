## Makefile for 'objextract'
#############################

APP = objextract
SRC = objextract

LIB = seqset $(SEQ_LIBS) pub medline biblio general xser xutil xncbi

REQUIRES = objects

WATCHERS = dicuccio
