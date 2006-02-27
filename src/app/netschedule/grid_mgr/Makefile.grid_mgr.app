# $Id$
# Author:  Maxim Didneko
#################################

APP = grid_mgr.cgi
SRC = mgrapp mgrres mgrcmd mgrlogic
LIB = ncbi_xblobstorage_netcache xconnserv xconnect xutil xhtml xcgi xutil xncbi 
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

