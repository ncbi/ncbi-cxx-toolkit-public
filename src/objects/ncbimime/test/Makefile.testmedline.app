#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build serialization test application "serialtest"
#################################

APP = testmedline
SRC = testmedline
LIB = ncbimime mmdb1 mmdb2 mmdb3 seqset $(SEQ_LIBS) \
	pub medline medlars biblio general \
	xser xutil xncbi

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(ORIG_LIBS)
