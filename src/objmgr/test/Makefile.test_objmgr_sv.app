#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build object manager test application "test_objmgr"
#################################

APP = test_objmgr_sv
SRC = test_objmgr_sv
LIB = $(SOBJMGR_LIBS)

LIBS = $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_objmgr_sv -seed 1 -checksum f6dfc69d24d9bd793df456af53241b7f
