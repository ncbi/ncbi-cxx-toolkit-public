#################################
# $Id$
# Author:  Eugen Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build serialization test application "serialtest"
#################################

APP = asn2asn
OBJ = asn2asn
LIB = xobjmgr seqset seq seqres seqloc seqalign seqfeat seqblock xobjmgr \
	pub medline biblio general xser xncbi

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(ORIG_LIBS)
