#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build serialization test/demo application "asn2asn"
#################################

REQUIRES = objects dbapi FreeTDS

APP = asn2asn
SRC = asn2asn

LIB  = submit ncbi_xdbapi_ftds $(FTDS_LIB) $(OBJMGR_LIBS)
LIBS = $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = asn2asn.sh
CHECK_CMD = asn2asn.sh NCBI_TEST_DATA/objects
CHECK_COPY = asn2asn.sh ../../serial/datatool/testdata ../../check/ncbi_test_data
CHECK_REQUIRES = in-house-resources
CHECK_TIMEOUT = 600

WATCHERS = gouriano
