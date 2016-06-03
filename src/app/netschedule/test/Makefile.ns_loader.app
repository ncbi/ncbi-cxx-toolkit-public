# $Id$

APP = ns_loader
SRC = ns_loader
LIB = xconnserv xthrserv xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
REQUIRES = Linux

WATCHERS = satskyse
