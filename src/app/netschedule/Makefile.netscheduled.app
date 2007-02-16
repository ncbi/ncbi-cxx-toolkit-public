#################################
# $Id$
#################################

APP = netscheduled
SRC = netscheduled nslb bdb_queue job_status queue_clean_thread \
      notif_thread ns_affinity squeue access_list

REQUIRES = MT bdb


LIB =  $(BDB_LIB) xconnserv xthrserv xconnect xutil xncbi xqueryparse
LIBS = $(BERKELEYDB_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BERKELEYDB_INCLUDE) -DBMCOUNTOPT
