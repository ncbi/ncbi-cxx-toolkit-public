#################################
# $Id$
# Author:  Anatoliy Kuznetsov
#################################


REQUIRES = objects 

APP = test_biotree
SRC = test_biotree
LIB = xalgophytree biotree fastme taxon1 xalnmgr xobjutil xconnect tables \
      $(SOBJMGR_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

#CHECK_CMD = test_biotree
#CHECK_TIMEOUT = 500

WATCHERS = kuznets
