# $Id$
# Author:  Maxim Didneko
#################################

APP = remote_app
SRC = remote_app_wn exec_helpers

REQUIRES = MT

LIB = xconnserv xthrserv xconnect xutil xncbi 
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)


WATCHERS = sadyrovr
