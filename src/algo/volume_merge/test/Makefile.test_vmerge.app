#################################
# $Id$
# Author:  Anatoliy Kuznetsov
#################################



APP = test_vmerge
SRC = test_vmerge
LIB = xalgovmerge xutil xncbi

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)


