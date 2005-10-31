# $Id$
#################################

APP = bma_refiner

SRC = bma_refiner

REQUIRES = objects

LIB =   xbma_refiner \
        xstruct_util \
        xstruct_dp \
        xblast seqdb blastdb xnetblastcli xnetblast tables \
        xobjsimple xobjutil \
        cdd \
        scoremat \
        cn3d \
        mmdb \
        $(OBJMGR_LIBS)

CXXFLAGS   = $(FAST_CXXFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS) -I$(srcdir)/..

LDFLAGS  = $(FAST_LDFLAGS)

LIBS = $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)
