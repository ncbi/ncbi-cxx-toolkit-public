#################################
# $Id$
# Author:  Aleksey Grichenko (grichenk@ncbi.nlm.nih.gov)
#################################

# Build split data loader application "split_loader_demo"
#################################

REQUIRES = objects

APP = split_loader_demo
SRC = split_loader_demo split_loader
LIB = $(SOBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = vasilche
