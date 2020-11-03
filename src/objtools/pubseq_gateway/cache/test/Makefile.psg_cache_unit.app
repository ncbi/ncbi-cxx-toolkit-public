APP = psg_cache_unit
SRC = psg_cache_unit unit/psg_cache_base unit/psg_cache_bioseq_info unit/psg_cache_si2csi \
      unit/psg_cache_blobprop unit/psg_pack_unpack

WATCHERS = saprykin
REQUIRES = MT Linux LMDB PROTOBUF GMOCK

#COVERAGE_FLAGS=-fprofile-arcs -ftest-coverage

LOCAL_CPPFLAGS=-I$(import_root)/../include
CPPFLAGS = $(LMDB_INCLUDE) $(GMOCK_INCLUDE) $(ORIG_CPPFLAGS) $(COVERAGE_FLAGS)
LIB = $(SEQ_LIBS) pub medline biblio general psg_cache psg_cassandra psg_protobuf xser xconnect xutil $(LOCAL_LIB) xncbi
LIBS = $(LMDB_LIBS) $(PROTOBUF_LIBS) $(GMOCK_LIBS) $(ORIG_LIBS)
LOCAL_CPPFLAGS += -fno-delete-null-pointer-checks

LOCAL_LDFLAGS = -L$(import_root)/../lib
LDFLAGS = $(ORIG_LDFLAGS) $(FAST_LDFLAGS) $(LOCAL_LDFLAGS) $(COVERAGE_FLAGS)

CHECK_CMD = psg_cache_unit
