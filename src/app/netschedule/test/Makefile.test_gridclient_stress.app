# $Id$

APP = test_gridclient_stress
SRC = test_gridclient_stress
LIB = xconnserv$(FORCE_STATIC) xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

