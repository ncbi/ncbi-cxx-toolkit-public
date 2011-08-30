#################################
# $Id$
#################################

APP = test_source_mod_parser
SRC = test_source_mod_parser

LIB = xobjreadex xobjread xobjutil $(SOBJMGR_LIBS) creaders seqset $(SEQ_LIBS) pub medline biblio general \
      xser xutil xncbi
LIBS = $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = ucko
