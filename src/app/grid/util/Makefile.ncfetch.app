#################################
# $Id$
#################################

APP = ncfetch.cgi
SRC = ncfetch

LIB = xcgi xconnserv xthrserv xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = sadyrovr
