APP = agp_validate
SRC = agp_validate db

LIB = entrez2 entrez2cli taxon1 xobjutil $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

