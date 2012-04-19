# $Id$
#################################

APP = grid_cli
SRC = grid_cli nc_cmds ns_cmds adm_cmds ns_cmd_impl wn_cmds \
	automation json_over_uttp util
LIB = ncbi_xcache_netcache xconnserv xcgi xthrserv xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)


WATCHERS = kazimird
