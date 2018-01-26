# $Id$

APP = pubseq_gateway
SRC = AccVerCacheD AccVerCacheDB AccVerCacheStorage

LIBS = $(CASSANDRA_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(CASSANDRA_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = CASSANDRA MT Linux

WATCHERS = satskyse dmitrie1

#LIB = DdRpc h2o ssl crypto idcassandra xncbi connect IdLogUtil
#LIBS = $(LMDB_LIBS) -L./external/lib/linux/$(MODE) -L./external/lib64 -L./external/lib -lcassandra_static  -luv-static $(DL_LIBS)
