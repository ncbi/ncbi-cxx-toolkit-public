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

#CHECK_CMD = test_objmgr_sv -seed 1 -checksum c78cb2fb4d1b2926fede0945d9ae88b9 /CHECK_NAME=test_objmgr_sv
#CHECK_CMD = test_objmgr_sv -seed 1 -checksum bb50ff61b99170a94a0f264630c265e5 /CHECK_NAME=test_objmgr_sv
CHECK_CMD = test_objmgr_sv -seed 1 -checksum de21b79e1b948b6620333a8d1c6eaa87 /CHECK_NAME=test_objmgr_sv

WATCHERS = vasilche
