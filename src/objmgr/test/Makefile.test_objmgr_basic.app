#################################
# $Id$
#################################

REQUIRES = dbapi

APP = test_objmgr_basic
SRC = test_objmgr_basic
LIB = $(SOBJMGR_LIBS)

LIBS = $(ORIG_LIBS)

CHECK_CMD = test_objmgr_loaders.sh test_objmgr_basic
CHECK_COPY = test_objmgr_loaders.sh
