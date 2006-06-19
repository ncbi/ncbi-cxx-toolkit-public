#################################
# $Id$
#################################

REQUIRES = objects

APP = ace2asn
SRC = ace2asn

LIB = xobjread xobjutil $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

