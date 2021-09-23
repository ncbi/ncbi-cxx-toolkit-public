# $Id$

APP = test_psg_cache

SRC = test_psg_cache

#COVERAGE_FLAGS=-fprofile-arcs -ftest-coverage
CPPFLAGS = $(LMDB_INCLUDE) $(ORIG_CPPFLAGS) $(COVERAGE_FLAGS)
LIBS = $(LMDB_LIBS) $(PROTOBUF_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
LIB = $(SEQ_LIBS) pub medline biblio general psg_cache psg_cassandra psg_protobuf xser xconnect xutil xncbi 

LDFLAGS = $(ORIG_LDFLAGS) $(FAST_LDFLAGS) $(COVERAGE_FLAGS) $(LOCAL_LDFLAGS)

REQUIRES = CASSANDRA MT Linux LMDB PROTOBUF

WATCHERS = satskyse saprykin

user_homedir = $(shell readlink -e ~)
LOCAL_CPPFLAGS = -I$(user_homedir)/trunk/c++/include
