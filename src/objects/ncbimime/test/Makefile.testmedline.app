#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build test application "testmedline"
#################################

APP = testmedline
SRC = testmedline
LIB = ncbimime cdd cn3d mmdb1 mmdb2 mmdb3 seqset $(SEQ_LIBS) \
	pub medline medlars biblio general \
	xser xutil xncbi
