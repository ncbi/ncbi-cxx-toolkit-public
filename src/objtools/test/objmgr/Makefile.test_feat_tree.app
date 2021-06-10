# $Id$

APP = test_feat_tree
SRC = test_feat_tree

CPPFLAGS = $(ORIG_CPPFLAGS)

LIB = xobjutil $(OBJMGR_LIBS)
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_COPY = test_feat_tree.sh
CHECK_CMD = test_feat_tree.sh ../../check/ncbi_test_data
CHECK_REQUIRES = in-house-resources

WATCHERS = vasilche
