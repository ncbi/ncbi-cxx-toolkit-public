#################################
# $Id$
# Author:  Anatoliy Kuznetsov
#################################

# Build object manager test application "test_objmgr"
#################################

REQUIRES = objects 

APP = test_biotree
SRC = test_biotree
LIB = $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

#CHECK_CMD = test_seqvector_ci
#CHECK_TIMEOUT = 500
