#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build test application "test_ncbimime"
#################################

APP = test_ncbimime
SRC = test_ncbimime
LIB = ncbimime cdd cn3d mmdb seqset $(SEQ_LIBS) \
	pub medline medlars biblio general \
	xser xutil xncbi
