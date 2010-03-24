#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build serialization test/demo application "asn2asn"
#################################

REQUIRES = objects dbapi FreeTDS

APP = asn2asn
SRC = asn2asn
LIB = ncbi_xdbapi_ftds $(FTDS64_CTLIB_LIB) dbapi_driver$(STATIC) $(COMPRESS_LIBS) \
    seqset $(SEQ_LIBS) pub medline biblio general xser xutil xncbi
LIBS = $(FTDS64_CTLIB_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) \
       $(ORIG_LIBS)

CHECK_CMD = asn2asn.sh
CHECK_CMD = asn2asn.sh /am/ncbiapdata/test_data/objects
CHECK_COPY = asn2asn.sh ../../serial/datatool/testdata
CHECK_REQUIRES = unix in-house-resources -Cygwin
CHECK_TIMEOUT = 600

WATCHERS = gouriano
