# $Id$
#################################

APP = grid_cli
SRC = grid_cli nc_cmds ns_cmds adm_cmds automation json_over_uttp
LIB = ncbi_xcache_netcache xconnserv xthrserv xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)


WATCHERS = kazimird
