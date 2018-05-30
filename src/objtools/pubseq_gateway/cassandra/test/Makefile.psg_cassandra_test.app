APP = psg_cassandra_test
SRC = psg_cassandra_test blob_record_test

WATCHERS=saprykin

#COVERAGE_FLAGS=-fprofile-arcs -ftest-coverage
CPPFLAGS=$(ORIG_CPPFLAGS) $(GMOCK_INCLUDE) $(COVERAGE_FLAGS)

LIB=xncbi psg_cassandra$(STATIC) psg_diag $(LOCAL_LIB)
LIBS=$(GMOCK_LIBS) $(ORIG_LIBS)

LDFLAGS = $(ORIG_LDFLAGS) $(FAST_LDFLAGS) $(COVERAGE_FLAGS) $(LOCAL_LDFLAGS) 

CHECK_CMD = psg_cassandra_test

