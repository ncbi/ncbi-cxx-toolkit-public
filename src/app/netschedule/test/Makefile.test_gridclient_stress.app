# $Id$

APP = test_gridclient_stress
SRC = test_gridclient_stress
LIB = xconnserv$(FORCE_STATIC) xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_COPY = test_gridclient_stress.ini
