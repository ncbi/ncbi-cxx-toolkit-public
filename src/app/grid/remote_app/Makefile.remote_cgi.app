# $Id$
# Author:  Maxim Didneko
#################################

APP = remote_cgi
SRC = remote_cgi_wn exec_helpers

REQUIRES = MT

LIB = xconnserv xthrserv xconnect xcgi xutil xncbi 
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)


WATCHERS = kazimird sadyrovr
