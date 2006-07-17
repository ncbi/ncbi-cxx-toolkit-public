# $Id$

APP = ns_remote_job_control
SRC = ns_remote_job_control info_collector renderer
LIB = ncbi_xblobstorage_netcache xconnserv xthrserv xconnect xutil xncbi 

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

