# $Id$
#################################

WATCHERS = lanczyck

APP = bma_refiner

SRC = bma_refiner

REQUIRES = objects -Cygwin

LIB =   xbma_refiner \
        xcd_utils ncbimime taxon1 \
	xstruct_util \
	xstruct_dp \
	cdd \
	cn3d \
	mmdb \
	entrez2cli entrez2 \
	id1cli \
	xregexp $(PCRE_LIB) \
	$(BLAST_LIBS) \
	$(OBJMGR_LIBS)

CXXFLAGS   = $(FAST_CXXFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS) -I$(srcdir)/..

LDFLAGS  = $(FAST_LDFLAGS)

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(PCRE_LIBS) $(ORIG_LIBS)
