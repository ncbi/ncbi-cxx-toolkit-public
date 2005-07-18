# $Id$
#################################

APP = nrDemo
SRC = cuSimpleClusterer \
      cuSimpleSlc \
      cuSimpleNonRedundifier \
      nrDemo

LIB = xcd_utils \
      cdd cn3d mmdb scoremat seqset seq \
      pub medline biblio general taxon1 \
      xser xutil xconnect xncbi

LIBS =  $(ORIG_LIBS)

REQUIRES = objects
