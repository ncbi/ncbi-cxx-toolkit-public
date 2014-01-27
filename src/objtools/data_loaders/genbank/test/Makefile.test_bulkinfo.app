#################################
# $Id$
#################################

APP = test_bulkinfo
SRC = test_bulkinfo
LIB = xobjutil $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_COPY = bad_len.ids

CHECK_CMD = test_bulkinfo -type gi
CHECK_CMD = test_bulkinfo -type acc
CHECK_CMD = test_bulkinfo -type label
CHECK_CMD = test_bulkinfo -type taxid
CHECK_CMD = test_bulkinfo -type length
CHECK_CMD = test_bulkinfo -type type
CHECK_CMD = test_bulkinfo -type gi -idlist bad_len.ids
CHECK_CMD = test_bulkinfo -type acc -idlist bad_len.ids
CHECK_CMD = test_bulkinfo -type label -idlist bad_len.ids
CHECK_CMD = test_bulkinfo -type taxid -idlist bad_len.ids
CHECK_CMD = test_bulkinfo -type length -idlist bad_len.ids
CHECK_CMD = test_bulkinfo -type type -idlist bad_len.ids

WATCHERS = vasilche
