# $Id$

APP = grid_client_sample
SRC = grid_client_sample
LIB = xconnserv$(FORCE_STATIC) xconnect xutil xncbi 

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

