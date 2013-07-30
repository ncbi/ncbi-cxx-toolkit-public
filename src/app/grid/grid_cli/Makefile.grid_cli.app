# $Id$
#################################

APP = grid_cli
SRC = grid_cli nc_cmds ns_cmds adm_cmds ns_cmd_impl wn_cmds \
        misc_cmds automation util nst_cmds
LIB = ncbi_xcache_netcache netstorage xconnserv xcgi \
        xthrserv xconnect connssl xutil xncbi
LIBS = $(GNUTLS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = GNUTLS

CHECK_CMD = netcache_check.sh
CHECK_COPY = netcache_check.sh
CHECK_TIMEOUT = 350
CHECK_REQUIRES = in-house-resources

WATCHERS = kazimird
