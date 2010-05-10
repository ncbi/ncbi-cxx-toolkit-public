#################################
# $Id$
#################################

APP = ncfetch.cgi
SRC = ncfetch

LIB = xcgi xconnserv xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

WATCHERS = kazimird
