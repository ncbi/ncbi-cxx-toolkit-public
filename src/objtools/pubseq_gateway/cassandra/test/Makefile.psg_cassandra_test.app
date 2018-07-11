APP = psg_cassandra_test
SRC = psg_cassandra_test unit/blob_record unit/cluster_meta unit/fullscan_plan \
    unit/fullscan_runner unit/cassandra_query

WATCHERS=saprykin
REQUIRES = CASSANDRA MT Linux GCC

#COVERAGE_FLAGS=-fprofile-arcs -ftest-coverage
CPPFLAGS=$(ORIG_CPPFLAGS) $(GMOCK_INCLUDE) $(CASSANDRA_INCLUDE) $(COVERAGE_FLAGS)

MY_LIB=$(XCONNEXT) xconnect connext
LIB=$(MY_LIB:%=%$(STATIC)) psg_cassandra$(STATIC) psg_diag $(LOCAL_LIB) xncbi

LIBS= -lconnect $(GMOCK_LIBS) $(CASSANDRA_LIBS) $(ORIG_LIBS)

LDFLAGS = $(ORIG_LDFLAGS) $(FAST_LDFLAGS) $(COVERAGE_FLAGS) $(LOCAL_LDFLAGS) 

CHECK_CMD = psg_cassandra_test
