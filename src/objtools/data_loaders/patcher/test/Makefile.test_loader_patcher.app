#################################
# $Id$
#################################

REQUIRES = objects dbapi

APP = test_loader_patcher
SRC = test_loader_patcher

LOADER_PATCHER = ncbi_xloader_patcher

LIB = xobjmgr $(LOADER_PATCHER) $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

