# $Id$

APP = grid_worker_sample
SRC = grid_worker_sample
LIB = xconnserv$(FORCE_STATIC) xthrserv xconnect xutil xncbi 

REQUIRES = MT


LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

