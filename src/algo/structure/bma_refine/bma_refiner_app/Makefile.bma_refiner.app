# $Id$
#################################

APP = bma_refiner

SRC = bma_refiner

REQUIRES = objects

LIB =   xbma_refiner \
        xcd_utils ncbimime taxon1 \
        xstruct_util \
        xstruct_dp \
        xblast xalgoblastdbindex composition_adjustment \
        seqdb blastdb xnetblastcli xnetblast \
        tables \
        cdd \
        scoremat \
        cn3d \
        mmdb \
	entrez2cli entrez2 \
        id1cli id1 \
        xalgodustmask xobjread creaders \
        xobjsimple xobjutil \
        $(OBJMGR_LIBS)

CXXFLAGS   = $(FAST_CXXFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS) -I$(srcdir)/..

LDFLAGS  = $(FAST_LDFLAGS)

LIBS = $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)
