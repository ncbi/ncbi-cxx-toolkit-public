APP = psg_ipg_test
SRC = psg_ipg_test unit/fetch_report

WATCHERS=saprykin
REQUIRES = CASSANDRA MT Linux GCC

MY_LIB=psg_ipg psg_cassandra $(COMPRESS_LIBS) \
    xobjutil id2 seqsplit seqset $(SEQ_LIBS) pub medline biblio general xconnect xser xutil
LIB=$(MY_LIB:%=%$(STATIC)) xncbi

CPPFLAGS=$(ORIG_CPPFLAGS) $(GMOCK_INCLUDE) $(CASSANDRA_INCLUDE)
LIBS=$(NETWORK_LIBS) $(GMOCK_LIBS) $(CASSANDRA_STATIC_LIBS) $(OPENSSL_STATIC_LIBS) $(ORIG_LIBS)

LOCAL_CPPFLAGS+=-fno-delete-null-pointer-checks
LOCAL_LDFLAGS+=$(COVERAGE_FLAGS)
#COVERAGE_FLAGS=-fprofile-arcs -ftest-coverage
