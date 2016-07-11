# $Id$

APP = test_server_listeners
SRC = test_server_listeners
LIB = xthrserv xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify -best-effort CC

REQUIRES = MT Linux

WATCHERS = satskyse
