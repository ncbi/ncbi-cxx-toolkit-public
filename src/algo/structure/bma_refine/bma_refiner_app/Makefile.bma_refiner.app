# $Id$
#################################

APP = bma_refiner

SRC = bma_refiner

REQUIRES = objects

LIB =   xbma_refiner \
        xstruct_util \
        xstruct_dp \
        xblast composition_adjustment seqdb xobjread tables \
        cdd \
        scoremat \
        cn3d \
        mmdb \
        seqset seq seqcode sequtil \
        pub medline biblio general taxon1 blastdb xnetblast \
        xser xutil xconnect xncbi


#        tables xobjsimple xobjutil \
#        $(OBJMGR_LIBS)

CXXFLAGS   = $(FAST_CXXFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS) -I$(srcdir)/..

LDFLAGS  = $(FAST_LDFLAGS)

LIBS = $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)
