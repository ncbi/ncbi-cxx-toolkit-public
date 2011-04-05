#################################
# $Id$
#################################

WATCHERS = satskyse

APP = netscheduled
SRC = netscheduled queue_coll job_status queue_clean_thread \
      notif_thread ns_affinity ns_queue access_list ns_util ns_format \
      worker_node job

REQUIRES = MT bdb


LIB =  $(BDB_LIB) xconnserv xthrserv xconnect xqueryparse xutil xncbi
LIBS = $(BERKELEYDB_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BERKELEYDB_INCLUDE) -DBMCOUNTOPT
