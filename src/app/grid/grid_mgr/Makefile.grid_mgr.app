# $Id$
# Author:  Maxim Didneko
#################################

APP = grid_mgr.cgi
SRC = mgrapp mgrres mgrcmd mgrlogic
LIB = ncbi_xblobstorage_netcache xconnserv xthrserv xconnect xhtml xcgi \
      xutil xncbi 
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

