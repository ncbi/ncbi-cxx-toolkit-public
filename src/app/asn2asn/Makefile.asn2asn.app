#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build serialization test application "serialtest"
#################################

APP = asn2asn
SRC = asn2asn
LIB = seqset $(SEQ_LIBS) pub medline biblio general xser xutil xncbi

CHECK_CMD = asn2asn.sh
CHECK_CMD = asn2asn.sh /net/sampson/a/coremake/test_data/objects
