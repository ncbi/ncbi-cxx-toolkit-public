#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build serialization test application "serialtest"
#################################

APP = asn2asn
OBJ = asn2asn
LIB = seqset $(SEQ_LIBS) pub medline biblio general xser xutil xncbi

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(ORIG_LIBS)
