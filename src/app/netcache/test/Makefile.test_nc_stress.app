# $Id$

APP = test_nc_stress
SRC = test_nc_stress
LIB = xconnserv xthrserv xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = gouriano
