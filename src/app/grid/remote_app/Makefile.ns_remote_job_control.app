# $Id$

APP = ns_remote_job_control
SRC = ns_remote_job_control info_collector renderer
LIB = xconnserv xthrserv xconnect xutil xncbi 

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)


WATCHERS = sadyrovr
