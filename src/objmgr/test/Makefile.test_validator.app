###############################
# $Id$
###############################

APP = test_validator
SRC = test_validator
LIB = xobjutil xobjmgr id1 submit seqset $(SEQ_LIBS) pub medline biblio general \
      dbapi_driver xser xutil xconnect xncbi

LIBS = $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_validator
CHECK_COPY = current.prt
