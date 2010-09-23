#################################
# $Id$
#################################

APP = test_bulkinfo
SRC = test_bulkinfo
LIB = $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_bulkinfo -type gi
CHECK_CMD = test_bulkinfo -type acc
CHECK_CMD = test_bulkinfo -type label
CHECK_CMD = test_bulkinfo -type taxid

WATCHERS = vasilche
