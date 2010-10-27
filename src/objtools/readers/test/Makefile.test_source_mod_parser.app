#################################
# $Id$
#################################

APP = test_source_mod_parser
SRC = test_source_mod_parser

LIB = xobjread creaders seqset $(SEQ_LIBS) pub medline biblio general \
      xser xutil xncbi

WATCHERS = ucko
