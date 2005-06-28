# $Id$
#################################

APP = bma_refiner

SRC = bma_refiner

REQUIRES = objects ctools C-Toolkit

LIB =   xbma_refiner \
        xstruct_util \
        xstruct_dp \
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

CPPFLAGS = $(ORIG_CPPFLAGS) -I$(srcdir)/..  $(NCBI_C_INCLUDE)

#LDFLAGS  = $(FAST_LDFLAGS)

NCBI_C_LIBS = -lncbimmdb -lncbiid1 -lnetcli -lncbitool -lncbiobj -lncbi

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) 
