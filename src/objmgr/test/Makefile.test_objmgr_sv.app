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

CHECK_CMD = test_objmgr_sv -seed 1 -checksum 0c9ce1aa3d6de12805a454aa1b9a9b7a
