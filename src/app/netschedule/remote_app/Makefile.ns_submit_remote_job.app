# $Id$

APP = ns_submit_remote_job 
SRC = ns_submit_remote_job
LIB = ncbi_xblobstorage_netcache xconnserv xconnect xutil xncbi 

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

