#################################
# $Id$
# Author:  Eugen Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build serialization test application "serialtest"
#################################

APP = asn2asn
OBJ = asn2asn
LIB = seqset seq seqres seqloc seqalign seqfeat seqblock pub medline medlars biblio general xser xncbi

LDFLAGS  = -xildoff -O

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(ORIG_LIBS)
