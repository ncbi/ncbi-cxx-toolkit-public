# $Id$
#################################

APP = bma_refiner

SRC = bma_refiner

REQUIRES = objects

LIB =   xbma_refiner \
        xstruct_util \
        xstruct_dp \
        xblast blastdb xnetblast tables \
        cdd \
        scoremat \
        cn3d \
        mmdb \
        seqset $(SEQ_LIBS) \
        pub \
        medline \
        biblio \
        general \
        xser \
        xutil \
        xconnect \
        xncbi 

#CFLAGS   = $(FAST_CFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS) -I$(srcdir)/..

#LDFLAGS  = $(FAST_LDFLAGS)

LIBS = $(ORIG_LIBS) $(NETWORK_LIBS)
