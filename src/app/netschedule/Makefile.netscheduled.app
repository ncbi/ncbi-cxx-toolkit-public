#################################
# $Id$
#################################

WATCHERS = satskyse

APP = netscheduled
SRC = netscheduled job_status queue_clean_thread \
      ns_affinity ns_queue access_list ns_util \
      job ns_server_misc ns_server_params ns_handler \
      ns_server ns_queue_parameters queue_database \
      ns_clients ns_command_arguments ns_clients_registry ns_notifications \
      ns_service_thread ns_group ns_gc_registry ns_statistics_counters \
      ns_rollback ns_alert ns_start_ids ns_perf_logging ns_db_dump \
      ns_scope ns_restore_state

REQUIRES = MT Linux


LIB =  xconnserv xthrserv xconnect xutil xncbi
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS)

