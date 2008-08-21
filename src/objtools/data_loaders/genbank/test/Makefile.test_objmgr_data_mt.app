#################################
# $Id$
#################################


APP = test_objmgr_data_mt
SRC = test_objmgr_data_mt
LIB = test_mt $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_objmgr_loaders.sh test_objmgr_data_mt.sh /CHECK_NAME=test_objmgr_data_mt.sh
CHECK_COPY = test_objmgr_loaders.sh test_objmgr_data_mt.sh
CHECK_TIMEOUT = 1000
