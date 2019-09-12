APP = psg_cache_unit
SRC = psg_cache_unit unit/psg_cache_base

WATCHERS = saprykin
REQUIRES = MT Linux LMDB PROTOBUF GMOCK

LOCAL_CPPFLAGS=-I$(import_root)/../include
CPPFLAGS = $(LMDB_INCLUDE) $(PROTOBUF_INCLUDE) $(GMOCK_INCLUDE) $(ORIG_CPPFLAGS)
LIB = $(SEQ_LIBS) pub medline biblio general psg_protobuf psg_cache  xser xutil $(LOCAL_LIB) xncbi
LIBS = $(LMDB_LIBS) $(PROTOBUF_LIBS) $(GMOCK_LIBS) $(ORIG_LIBS)
LOCAL_CPPFLAGS += -fno-delete-null-pointer-checks

LOCAL_LDFLAGS = -L$(import_root)/../lib
LDFLAGS = $(ORIG_LDFLAGS) $(FAST_LDFLAGS) $(LOCAL_LDFLAGS)

#CHECK_CMD = psg_cache_unit
