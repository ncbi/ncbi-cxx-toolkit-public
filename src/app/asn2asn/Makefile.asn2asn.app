#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build serialization test/demo application "asn2asn"
#################################

APP = asn2asn
SRC = asn2asn
LIB = seqset $(SEQ_LIBS) pub medline biblio general xser xutil xncbi

REQUIRES = objects

CHECK_CMD = asn2asn.sh
CHECK_CMD = asn2asn.sh /am/ncbiapdata/test_data/objects
CHECK_COPY = asn2asn.sh ../../serial/datatool/testdata
CHECK_REQUIRES = unix
CHECK_TIMEOUT = 600
