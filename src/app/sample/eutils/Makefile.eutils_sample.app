#################################
# $Id$
#################################

APP = eutils_sample
SRC = eutils_sample

LIB = $(EUTILS_LIBS) seqset $(SEQ_LIBS) pub medline biblio general \
      xser xcgi xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects
