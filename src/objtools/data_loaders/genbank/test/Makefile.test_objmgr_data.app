#################################
# $Id$
#################################


APP = test_objmgr_data
SRC = test_objmgr_data
LIB = $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_objmgr_loaders.sh test_objmgr_data.sh
CHECK_COPY = test_objmgr_loaders.sh test_objmgr_data.sh
